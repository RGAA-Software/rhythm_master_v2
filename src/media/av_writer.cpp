#include "rhythm/media/av_writer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "local_output.h"

extern "C" {
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

// Encoding/draining and stream time-base conversion adapt FFmpeg n6.1.1
// doc/examples/mux.c (Copyright 2003 Fabrice Bellard, MIT). See the retained
// third_party/notices/ffmpeg-examples/LICENSE.txt and provenance/media_encoding.json.
namespace rhythm::media {
namespace {
struct OutputFormatDelete {
    void operator()(AVFormatContext* value) const { avformat_free_context(value); }
};
struct ScalerDelete {
    void operator()(SwsContext* value) const { sws_freeContext(value); }
};
class EncodedTrack final {
   public:
    explicit EncodedTrack(std::string_view name = {}) {
        if (name.empty()) return;
        const auto* encoder = avcodec_find_encoder_by_name(std::string(name).c_str());
        if (!encoder) throw std::runtime_error("media.export_encoder_unavailable");
        codec_.reset(avcodec_alloc_context3(encoder));
        packet_.reset(av_packet_alloc());
        if (!codec_ || !packet_) throw std::bad_alloc();
    }
    AVCodecContext& Codec() { return *codec_; }
    void Open(AVFormatContext& format) {
        if (format.oformat->flags & AVFMT_GLOBALHEADER)
            codec_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        codec_->thread_count = 2;
        detail::Check(avcodec_open2(codec_.get(), nullptr, nullptr), "open export encoder");
        // The format owns each AVStream. Only its stable index is stored here.
        auto* stream = avformat_new_stream(&format, nullptr);
        if (!stream) throw std::bad_alloc();
        stream->time_base = codec_->time_base;
        if (codec_->codec_type == AVMEDIA_TYPE_VIDEO) stream->avg_frame_rate = codec_->framerate;
        index_ = stream->index;
        detail::Check(avcodec_parameters_from_context(stream->codecpar, codec_.get()),
                      "export stream parameters");
    }
    void Send(AVFrame& frame, AVFormatContext& format) {
        detail::Check(avcodec_send_frame(codec_.get(), &frame), "send export frame");
        Receive(format);
    }
    void Drain(AVFormatContext& format) {
        detail::Check(avcodec_send_frame(codec_.get(), nullptr), "drain export encoder");
        Receive(format);
    }

   private:
    void Receive(AVFormatContext& format) {
        while (true) {
            const int result = avcodec_receive_packet(codec_.get(), packet_.get());
            if (result == AVERROR(EAGAIN) || result == AVERROR_EOF) return;
            detail::Check(result, "receive encoded packet");
            av_packet_rescale_ts(packet_.get(), codec_->time_base,
                                 format.streams[index_]->time_base);
            packet_->stream_index = index_;
            detail::Check(av_interleaved_write_frame(&format, packet_.get()), "mux encoded packet");
        }
    }
    std::unique_ptr<AVCodecContext, detail::CodecDelete> codec_{};
    std::unique_ptr<AVPacket, detail::PacketDelete> packet_{};
    int index_ = -1;
};
class VideoEncoder final {
   public:
    VideoEncoder(const EncodingSettings& settings, AVFormatContext& format)
        : track_(settings.codec_ == VideoCodec::kH264 ? "h264_mf" : "mpeg4"), settings_(settings) {
        auto& codec = track_.Codec();
        codec.width = static_cast<int>(settings.width_);
        codec.height = static_cast<int>(settings.height_);
        codec.time_base = {1, static_cast<int>(settings.fps_)};
        codec.framerate = {static_cast<int>(settings.fps_), 1};
        codec.bit_rate = settings.bitrate_;
        codec.gop_size = static_cast<int>(settings.fps_ * 2);
        codec.max_b_frames = 0;
        codec.pix_fmt = settings.codec_ == VideoCodec::kH264 ? AV_PIX_FMT_NV12 : AV_PIX_FMT_YUV420P;
        codec.colorspace = AVCOL_SPC_BT709;
        codec.color_primaries = AVCOL_PRI_BT709;
        codec.color_trc = AVCOL_TRC_BT709;
        codec.color_range = AVCOL_RANGE_MPEG;
        if (settings.codec_ == VideoCodec::kH264) {
            // MF's camera_record scenario requests CFR instead of its dropping
            // VFR behavior. This is a Windows candidate until round-trip tests pass.
            codec.profile = AV_PROFILE_H264_MAIN;
            detail::Check(av_opt_set(codec.priv_data, "scenario", "camera_record", 0),
                          "constant-rate H264 export");
        }
        track_.Open(format);
        frame_.reset(av_frame_alloc());
        if (!frame_) throw std::bad_alloc();
        frame_->format = codec.pix_fmt;
        frame_->width = codec.width;
        frame_->height = codec.height;
        frame_->colorspace = codec.colorspace;
        frame_->color_primaries = codec.color_primaries;
        frame_->color_trc = codec.color_trc;
        frame_->color_range = codec.color_range;
        detail::Check(av_frame_get_buffer(frame_.get(), 32), "allocate export video frame");
        scaler_.reset(sws_getContext(codec.width, codec.height, AV_PIX_FMT_RGBA, codec.width,
                                     codec.height, codec.pix_fmt, SWS_BILINEAR, nullptr, nullptr,
                                     nullptr));
        if (!scaler_) throw std::bad_alloc();
        const auto* coefficients = sws_getCoefficients(SWS_CS_ITU709);
        detail::Check(sws_setColorspaceDetails(scaler_.get(), coefficients, 1, coefficients, 0, 0,
                                               1 << 16, 1 << 16),
                      "export SDR color conversion");
    }
    void Write(std::span<const std::uint8_t> rgba, AVFormatContext& format) {
        if (rgba.size() != static_cast<std::size_t>(settings_.width_) * settings_.height_ * 4)
            throw std::invalid_argument("media.export_frame_size");
        detail::Check(av_frame_make_writable(frame_.get()), "writable export video frame");
        const std::array<const std::uint8_t*, 4> source{rgba.data(), nullptr, nullptr, nullptr};
        const std::array<int, 4> stride{static_cast<int>(settings_.width_ * 4), 0, 0, 0};
        const int rows =
                sws_scale(scaler_.get(), source.data(), stride.data(), 0,
                          static_cast<int>(settings_.height_), frame_->data, frame_->linesize);
        if (rows != static_cast<int>(settings_.height_))
            throw std::runtime_error("media.export_conversion");
        frame_->pts = static_cast<std::int64_t>(frames_);
        frame_->duration = 1;
        track_.Send(*frame_, format);
        ++frames_;
    }
    void Finish(AVFormatContext& format) { track_.Drain(format); }
    std::uint64_t Frames() const { return frames_; }

   private:
    EncodedTrack track_{};
    EncodingSettings settings_{};
    std::unique_ptr<AVFrame, detail::FrameDelete> frame_{};
    std::unique_ptr<SwsContext, ScalerDelete> scaler_{};
    std::uint64_t frames_ = 0;
};
class AudioEncoder final {
   public:
    explicit AudioEncoder(AVFormatContext& format) {
        auto& codec = track_.Codec();
        codec.sample_rate = 48000;
        codec.time_base = {1, 48000};
        codec.bit_rate = 192000;
        codec.sample_fmt = AV_SAMPLE_FMT_FLTP;
        codec.ch_layout = AV_CHANNEL_LAYOUT_STEREO;
        track_.Open(format);
        if (codec.frame_size < 1 || codec.frame_size > 4096)
            throw std::runtime_error("media.export_audio_frame_size");
        frame_.reset(av_frame_alloc());
        if (!frame_) throw std::bad_alloc();
        frame_->format = codec.sample_fmt;
        frame_->sample_rate = codec.sample_rate;
        frame_->nb_samples = codec.frame_size;
        detail::Check(av_channel_layout_copy(&frame_->ch_layout, &codec.ch_layout),
                      "export audio layout");
        detail::Check(av_frame_get_buffer(frame_.get(), 0), "allocate export audio frame");
        SwrContext* allocated = nullptr;
        const int result =
                swr_alloc_set_opts2(&allocated, &codec.ch_layout, codec.sample_fmt, 48000,
                                    &codec.ch_layout, AV_SAMPLE_FMT_FLT, 48000, 0, nullptr);
        resampler_.reset(allocated);
        detail::Check(result, "allocate export audio conversion");
        if (!resampler_) throw std::bad_alloc();
        detail::Check(swr_init(resampler_.get()), "initialize export audio conversion");
        pending_.reserve(static_cast<std::size_t>(codec.frame_size) * 2);
    }
    void Write(std::span<const float> samples, AVFormatContext& format) {
        if (samples.empty() || samples.size() % 2 || samples.size() > 8192 ||
            std::any_of(samples.begin(), samples.end(),
                        [](float value) { return !std::isfinite(value); }))
            throw std::invalid_argument("media.export_audio_block");
        frames_ += samples.size() / 2;
        const auto capacity = static_cast<std::size_t>(track_.Codec().frame_size) * 2;
        while (!samples.empty()) {
            const auto count = std::min(capacity - pending_.size(), samples.size());
            pending_.insert(pending_.end(), samples.begin(), samples.begin() + count);
            samples = samples.subspan(count);
            if (pending_.size() == capacity) Encode(format);
        }
    }
    void Finish(AVFormatContext& format) {
        if (!pending_.empty()) Encode(format);
        track_.Drain(format);
    }
    std::uint64_t Frames() const { return frames_; }

   private:
    void Encode(AVFormatContext& format) {
        detail::Check(av_frame_make_writable(frame_.get()), "writable export audio frame");
        frame_->nb_samples = static_cast<int>(pending_.size() / 2);
        std::array<const std::uint8_t*, 1> source{
                reinterpret_cast<const std::uint8_t*>(pending_.data())};
        const int converted = swr_convert(resampler_.get(), frame_->data, frame_->nb_samples,
                                          source.data(), frame_->nb_samples);
        if (converted != frame_->nb_samples)
            throw std::runtime_error("media.export_audio_conversion");
        frame_->pts = static_cast<std::int64_t>(encoded_);
        track_.Send(*frame_, format);
        encoded_ += frame_->nb_samples;
        pending_.clear();
    }
    EncodedTrack track_{"aac"};
    std::unique_ptr<AVFrame, detail::FrameDelete> frame_{};
    std::unique_ptr<SwrContext, detail::ResamplerDelete> resampler_{};
    std::vector<float> pending_{};
    std::uint64_t frames_ = 0;
    std::uint64_t encoded_ = 0;
};
}  // namespace
class AvWriter::Impl final {
   public:
    Impl(const std::filesystem::path& path, EncodingSettings settings, std::stop_token stop)
        : settings_(settings) {
        if (!settings.width_ || !settings.height_ || settings.width_ > 4096 ||
            settings.height_ > 4096 || settings.width_ % 2 || settings.height_ % 2 ||
            static_cast<std::uint64_t>(settings.width_) * settings.height_ > 2073600 ||
            (settings.fps_ != 24 && settings.fps_ != 25 && settings.fps_ != 30 &&
             settings.fps_ != 60) ||
            settings.bitrate_ < 100000 || settings.bitrate_ > 50000000 ||
            (settings.codec_ != VideoCodec::kMpeg4 && settings.codec_ != VideoCodec::kH264))
            throw std::invalid_argument("media.export_settings");
        output_.emplace(path, stop);
        AVFormatContext* allocated = nullptr;
        const int result = avformat_alloc_output_context2(&allocated, nullptr, "mp4", nullptr);
        format_.reset(allocated);
        detail::Check(result, "allocate MP4 output");
        if (!format_) throw std::bad_alloc();
        format_->pb = &output_->Context();
        format_->flags |= AVFMT_FLAG_CUSTOM_IO;
        format_->max_interleave_delta = AV_TIME_BASE;
        // The default 1 kHz movie clock rounds edit-list durations. 48 kHz
        // exactly represents both stereo samples and every supported frame rate.
        detail::Check(av_opt_set_int(format_->priv_data, "movie_timescale", 48000, 0),
                      "sample-accurate MP4 edit duration");
        video_.emplace(settings_, *format_);
        if (settings_.audio_) audio_.emplace(*format_);
        detail::Check(avformat_write_header(format_.get(), nullptr), "write MP4 header");
    }
    void WriteVideo(std::span<const std::uint8_t> rgba) {
        Perform([&] {
            if (video_->Frames() >= settings_.fps_ * 3600ULL ||
                (audio_ &&
                 (video_->Frames() + 1) * 48000 > (audio_->Frames() + 48000) * settings_.fps_))
                throw std::invalid_argument("media.export_interleave");
            video_->Write(rgba, *format_);
        });
    }
    void WriteAudio(std::span<const float> stereo) {
        Perform([&] {
            if (stereo.empty() || stereo.size() % 2 || stereo.size() > 8192)
                throw std::invalid_argument("media.export_audio_block");
            if (!audio_ || (audio_->Frames() + stereo.size() / 2) * settings_.fps_ >
                                   (video_->Frames() + settings_.fps_) * 48000)
                throw std::invalid_argument("media.export_interleave");
            audio_->Write(stereo, *format_);
        });
    }
    void Finish() {
        Perform([&] {
            if (!video_->Frames() ||
                (audio_ && audio_->Frames() * settings_.fps_ != video_->Frames() * 48000))
                throw std::invalid_argument("media.export_duration_mismatch");
            video_->Finish(*format_);
            if (audio_) audio_->Finish(*format_);
            detail::Check(av_write_trailer(format_.get()), "write MP4 trailer");
            output_->Flush();
            finished_ = true;
        });
    }

   private:
    template <typename Function>
    void Perform(Function function) {
        if (failed_ || finished_) throw std::logic_error("media.export_closed");
        try {
            if (output_->Canceled()) throw std::runtime_error("media.export_canceled");
            function();
        } catch (...) {
            failed_ = true;
            throw;
        }
    }
    EncodingSettings settings_{};
    std::optional<detail::LocalOutput> output_{};
    std::unique_ptr<AVFormatContext, OutputFormatDelete> format_{};
    std::optional<VideoEncoder> video_{};
    std::optional<AudioEncoder> audio_{};
    bool failed_ = false;
    bool finished_ = false;
};
AvWriter::AvWriter(const std::filesystem::path& staging, EncodingSettings settings,
                   std::stop_token stop)
    : impl_(std::make_unique<Impl>(staging, settings, stop)) {}
AvWriter::~AvWriter() = default;
void AvWriter::WriteVideo(std::span<const std::uint8_t> rgba) { impl_->WriteVideo(rgba); }
void AvWriter::WriteAudio(std::span<const float> stereo) { impl_->WriteAudio(stereo); }
void AvWriter::Finish() { impl_->Finish(); }
}  // namespace rhythm::media
