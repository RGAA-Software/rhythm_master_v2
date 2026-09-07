#include "rhythm/media/video_decoder.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <utility>

#include "ffmpeg_resources.h"
#include "local_input.h"

extern "C" {
#include <libavutil/display.h>
#include <libswscale/swscale.h>
}

namespace rhythm::media {
namespace {
constexpr std::int64_t kMaximumPixels = 2073600;
struct ScalerDelete {
    void operator()(SwsContext* value) const { sws_freeContext(value); }
};
struct DictionaryDelete {
    void operator()(AVDictionary* value) const { av_dict_free(&value); }
};
void InspectVideo(AVFormatContext& format) {
    if (format.nb_streams > 16) throw std::runtime_error("video stream budget");
    std::array<std::unique_ptr<AVDictionary, DictionaryDelete>, 16> owned{};
    for (unsigned index = 0; index < format.nb_streams; ++index) {
        AVDictionary* options = nullptr;
        const int result = av_dict_set_int(&options, "max_pixels", kMaximumPixels, 0);
        owned[index].reset(options);
        detail::Check(result, "video probe pixel budget");
    }
    // FFmpeg may replace each dictionary. Ownership transfers only for this
    // synchronous C call and is recovered before checking its result.
    std::array<AVDictionary*, 16> options{};
    const auto count = format.nb_streams;
    for (unsigned index = 0; index < count; ++index) options[index] = owned[index].release();
    const int result = avformat_find_stream_info(&format, options.data());
    for (unsigned index = 0; index < count; ++index) owned[index].reset(options[index]);
    detail::Check(result, "inspect video");
}
void ValidateExtent(int width, int height) {
    if (width < 1 || height < 1 || width > 4096 || height > 4096 ||
        static_cast<std::int64_t>(width) * height > kMaximumPixels) {
        throw std::runtime_error("video extent exceeds the SDR 2-megapixel profile");
    }
}
}  // namespace

// Send/receive/drain follows FFmpeg n6.1.1 doc/examples/demux_decode.c (MIT,
// Stefano Sabatini). Native ownership, local I/O, timestamp selection, budgets
// and color conversion adapt that sequence to the project contracts.
class VideoDecoder::Impl final {
   public:
    Impl(const std::filesystem::path& path, std::uint64_t generation, std::stop_token stop)
        : path_(path),
          input_(std::make_unique<detail::LocalInput>(path, stop)),
          generation_(generation) {
        Open();
    }
    Impl(std::shared_ptr<const std::vector<std::uint8_t>> bytes, std::uint64_t generation,
         std::stop_token stop)
        : bytes_(std::move(bytes)),
          input_(std::make_unique<detail::LocalInput>(bytes_, stop)),
          generation_(generation) {
        Open();
    }
    void Open() {
        format_.reset(avformat_alloc_context());
        if (!format_) throw std::bad_alloc();
        format_->pb = &input_->Context();
        format_->flags |= AVFMT_FLAG_CUSTOM_IO;
        format_->probesize = 1024 * 1024;
        format_->max_analyze_duration = 5 * AV_TIME_BASE;
        format_->max_streams = 16;
        format_->max_probe_packets = 64;
        format_->interrupt_callback = {
                [](void* opaque) -> int {
                    return static_cast<detail::LocalInput*>(opaque)->Canceled() ? 1 : 0;
                },
                input_.get()};
        format_->protocol_whitelist = av_strdup("");
        format_->format_whitelist =
                av_strdup("mov,matroska,avi,ogg,png_pipe,jpeg_pipe,webp_pipe,bmp_pipe");
        if (!format_->protocol_whitelist || !format_->format_whitelist) throw std::bad_alloc();
        auto* opened = format_.release();
        const int opened_result = avformat_open_input(&opened, nullptr, nullptr, nullptr);
        format_.reset(opened);
        detail::Check(opened_result, "open video");
        InspectVideo(*format_);
        stream_ = av_find_best_stream(format_.get(), AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
        detail::Check(stream_, "find video stream");
        const auto& stream = *format_->streams[stream_];
        const auto* decoder = avcodec_find_decoder(stream.codecpar->codec_id);
        if (!decoder) throw std::runtime_error("video decoder unavailable");
        ValidateExtent(stream.codecpar->width, stream.codecpar->height);
        codec_.reset(avcodec_alloc_context3(decoder));
        if (!codec_) throw std::bad_alloc();
        detail::Check(avcodec_parameters_to_context(codec_.get(), stream.codecpar),
                      "video parameters");
        codec_->thread_count = 2;
        codec_->max_pixels = kMaximumPixels;
        detail::Check(avcodec_open2(codec_.get(), decoder, nullptr), "open video decoder");
        info_.width_ = static_cast<std::uint32_t>(codec_->width);
        info_.height_ = static_cast<std::uint32_t>(codec_->height);
        const auto aspect =
                av_guess_sample_aspect_ratio(format_.get(), format_->streams[stream_], nullptr);
        if (aspect.num > 0 && aspect.den > 0) info_.pixel_aspect_ = av_q2d(aspect);
        if (const auto* matrix = av_packet_side_data_get(stream.codecpar->coded_side_data,
                                                         stream.codecpar->nb_coded_side_data,
                                                         AV_PKT_DATA_DISPLAYMATRIX);
            matrix && matrix->size >= 9 * sizeof(std::int32_t)) {
            std::array<std::int32_t, 9> aligned{};
            std::memcpy(aligned.data(), matrix->data, sizeof(aligned));
            info_.clockwise_rotation_ = -av_display_rotation_get(aligned.data());
            if (!std::isfinite(info_.clockwise_rotation_))
                throw std::runtime_error("invalid video rotation");
        }
        time_base_ = av_q2d(stream.time_base);
        if (!std::isfinite(time_base_) || time_base_ <= 0)
            throw std::runtime_error("invalid video time base");
        start_seconds_ = stream.start_time == AV_NOPTS_VALUE ? 0 : stream.start_time * time_base_;
        const auto rate = av_guess_frame_rate(format_.get(), format_->streams[stream_], nullptr);
        if (rate.num > 0 && rate.den > 0) fallback_duration_ = av_q2d(av_inv_q(rate));
        if (stream.duration != AV_NOPTS_VALUE && stream.duration >= 0) {
            info_.duration_seconds_ = stream.duration * time_base_;
        } else if (format_->duration != AV_NOPTS_VALUE && format_->duration >= 0) {
            info_.duration_seconds_ = static_cast<double>(format_->duration) / AV_TIME_BASE;
        }
        packet_.reset(av_packet_alloc());
        frame_.reset(av_frame_alloc());
        if (!packet_ || !frame_) throw std::bad_alloc();
    }

    const std::filesystem::path& Path() const { return path_; }
    const std::shared_ptr<const std::vector<std::uint8_t>>& Bytes() const { return bytes_; }
    VideoInfo Info() const { return info_; }
    std::optional<VideoFrame> Read(std::stop_token stop) {
        if (stop.stop_requested()) throw std::runtime_error("media operation canceled");
        if (failed_)
            throw std::runtime_error("video read failed; seek or reopen before continuing");
        if (pending_) return std::exchange(pending_, std::nullopt);
        try {
            input_->SetStop(stop);
            av_frame_unref(frame_.get());
            if (!Receive()) return std::nullopt;
            return Convert();
        } catch (...) {
            failed_ = true;
            throw;
        }
    }
    void Skip(double seconds, std::stop_token stop) {
        while (auto frame = Read(stop)) {
            if (frame->seconds_ + 1e-9 >= seconds) {
                pending_ = std::move(frame);
                return;
            }
        }
        if (!info_.duration_seconds_ || seconds > *info_.duration_seconds_ + 1e-9) {
            throw std::out_of_range("video seek is past decoded EOF");
        }
    }

   private:
    bool Receive() {
        for (;;) {
            if (input_->Canceled()) throw std::runtime_error("media operation canceled");
            const int received = avcodec_receive_frame(codec_.get(), frame_.get());
            if (received == 0) return true;
            if (received == AVERROR_EOF) return false;
            if (received != AVERROR(EAGAIN)) detail::Check(received, "receive video frame");
            if (sent_eof_) throw std::runtime_error("video decoder failed to drain");
            for (;;) {
                if (input_->Canceled()) throw std::runtime_error("media operation canceled");
                av_packet_unref(packet_.get());
                const int read = av_read_frame(format_.get(), packet_.get());
                if (read == AVERROR_EOF) {
                    detail::Check(avcodec_send_packet(codec_.get(), nullptr), "drain video");
                    sent_eof_ = true;
                    break;
                }
                detail::Check(read, "read video packet");
                if (packet_->stream_index != stream_) continue;
                if (packet_->size > 16 * 1024 * 1024)
                    throw std::runtime_error("video packet exceeds budget");
                detail::Check(avcodec_send_packet(codec_.get(), packet_.get()),
                              "send video packet");
                break;
            }
        }
    }
    VideoFrame Convert() {
        ValidateExtent(frame_->width, frame_->height);
        if (frame_->width != static_cast<int>(info_.width_) ||
            frame_->height != static_cast<int>(info_.height_)) {
            throw std::runtime_error("video dimensions changed");
        }
        if (frame_->color_trc == AVCOL_TRC_SMPTE2084 ||
            frame_->color_trc == AVCOL_TRC_ARIB_STD_B67 ||
            frame_->color_primaries == AVCOL_PRI_BT2020) {
            throw std::runtime_error("HDR/wide-gamut video is outside the current SDR profile");
        }
        const int matrix = frame_->colorspace == AVCOL_SPC_BT709 ? SWS_CS_ITU709 : SWS_CS_ITU601;
        if (frame_->colorspace != AVCOL_SPC_UNSPECIFIED && frame_->colorspace != AVCOL_SPC_RGB &&
            frame_->colorspace != AVCOL_SPC_BT709 && frame_->colorspace != AVCOL_SPC_BT470BG &&
            frame_->colorspace != AVCOL_SPC_SMPTE170M) {
            throw std::runtime_error("unsupported video color matrix");
        }
        if (!scaler_ || format_value_ != frame_->format) {
            scaler_.reset(sws_getContext(frame_->width, frame_->height,
                                         static_cast<AVPixelFormat>(frame_->format), frame_->width,
                                         frame_->height, AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr,
                                         nullptr, nullptr));
            if (!scaler_) throw std::runtime_error("cannot create video conversion");
            format_value_ = frame_->format;
        }
        const auto* coefficients = sws_getCoefficients(matrix);
        detail::Check(sws_setColorspaceDetails(scaler_.get(), coefficients,
                                               frame_->color_range == AVCOL_RANGE_JPEG,
                                               coefficients, 1, 0, 1 << 16, 1 << 16),
                      "set video color conversion");
        VideoFrame output{};
        output.info_ = info_;
        output.rgba_.resize(static_cast<std::size_t>(info_.width_) * info_.height_ * 4);
        std::array<std::uint8_t*, 4> planes{output.rgba_.data(), nullptr, nullptr, nullptr};
        std::array<int, 4> strides{static_cast<int>(info_.width_ * 4), 0, 0, 0};
        const int rows = sws_scale(scaler_.get(), frame_->data, frame_->linesize, 0, frame_->height,
                                   planes.data(), strides.data());
        if (rows != frame_->height) throw std::runtime_error("incomplete video conversion");
        output.seconds_ = frame_->best_effort_timestamp == AV_NOPTS_VALUE
                                  ? next_seconds_
                                  : std::max(0.0, frame_->best_effort_timestamp * time_base_ -
                                                          start_seconds_);
        output.duration_seconds_ =
                frame_->duration > 0 ? frame_->duration * time_base_ : fallback_duration_;
        if (!std::isfinite(output.seconds_) || output.seconds_ + 1e-9 < previous_seconds_) {
            throw std::runtime_error("nonmonotonic video timestamps");
        }
        previous_seconds_ = output.seconds_;
        next_seconds_ = output.seconds_ + output.duration_seconds_;
        output.generation_ = generation_;
        return output;
    }

    std::filesystem::path path_{};
    std::shared_ptr<const std::vector<std::uint8_t>> bytes_{};
    std::unique_ptr<detail::LocalInput> input_{};
    std::unique_ptr<AVFormatContext, detail::FormatDelete> format_{};
    std::unique_ptr<AVCodecContext, detail::CodecDelete> codec_{};
    std::unique_ptr<AVPacket, detail::PacketDelete> packet_{};
    std::unique_ptr<AVFrame, detail::FrameDelete> frame_{};
    std::unique_ptr<SwsContext, ScalerDelete> scaler_{};
    VideoInfo info_{};
    std::optional<VideoFrame> pending_{};
    std::uint64_t generation_ = 0;
    int stream_ = -1;
    int format_value_ = -1;
    double time_base_ = 0;
    double start_seconds_ = 0;
    double next_seconds_ = 0;
    double previous_seconds_ = 0;
    double fallback_duration_ = 1.0 / 30;
    bool sent_eof_ = false;
    bool failed_ = false;
};

VideoDecoder::VideoDecoder(const std::filesystem::path& path, std::uint64_t generation,
                           std::stop_token stop)
    : impl_(std::make_unique<Impl>(path, generation, stop)) {}
VideoDecoder::~VideoDecoder() = default;
VideoDecoder::VideoDecoder(std::span<const std::uint8_t> bytes, std::uint64_t generation,
                           std::stop_token stop) {
    if (bytes.empty() || bytes.size() > 16 * 1024 * 1024)
        throw std::invalid_argument("embedded media byte budget");
    if (stop.stop_requested()) throw std::runtime_error("media operation canceled");
    impl_ = std::make_unique<Impl>(
            std::make_shared<const std::vector<std::uint8_t>>(bytes.begin(), bytes.end()),
            generation, stop);
}
VideoDecoder::VideoDecoder(VideoDecoder&&) noexcept = default;
VideoDecoder& VideoDecoder::operator=(VideoDecoder&&) noexcept = default;
VideoInfo VideoDecoder::Info() const { return impl_->Info(); }
std::optional<VideoFrame> VideoDecoder::Read(std::stop_token stop) { return impl_->Read(stop); }
void VideoDecoder::Seek(double seconds, std::uint64_t generation, std::stop_token stop) {
    if (!std::isfinite(seconds) || seconds < 0)
        throw std::invalid_argument("invalid video seek time");
    auto replacement = impl_->Bytes() ? std::make_unique<Impl>(impl_->Bytes(), generation, stop)
                                      : std::make_unique<Impl>(impl_->Path(), generation, stop);
    replacement->Skip(seconds, stop);
    impl_ = std::move(replacement);
}
}  // namespace rhythm::media
