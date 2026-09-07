#include "rhythm/media/audio_decoder.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

#include "ffmpeg_resources.h"
#include "local_input.h"

extern "C" {
#include <libavutil/avstring.h>
}

namespace rhythm::media {
namespace {
constexpr int kMaximumConvertedFrames = 1048576;
constexpr int kMaximumDecodedFrames = 262144;
}  // namespace

class AudioDecoder::Impl final {
   public:
    Impl(const std::filesystem::path& path, std::uint64_t generation, std::stop_token stop)
        : path_(path),
          input_(std::make_unique<detail::LocalInput>(path, stop)),
          generation_(generation) {
        Initialize();
    }
    Impl(std::shared_ptr<const std::vector<std::uint8_t>> bytes, std::uint64_t generation,
         std::stop_token stop)
        : bytes_(std::move(bytes)),
          input_(std::make_unique<detail::LocalInput>(bytes_, stop)),
          generation_(generation) {
        Initialize();
    }
    std::unique_ptr<Impl> Reopen(std::uint64_t generation, std::stop_token stop) const {
        if (file_bytes_.Valid()) return std::make_unique<Impl>(file_bytes_, generation, stop);
        return bytes_ ? std::make_unique<Impl>(bytes_, generation, stop)
                      : std::make_unique<Impl>(path_, generation, stop);
    }
    Impl(storage::FileBytes bytes, std::uint64_t generation, std::stop_token stop)
        : file_bytes_(std::move(bytes)),
          input_(std::make_unique<detail::LocalInput>(file_bytes_, stop)),
          generation_(generation) {
        Initialize();
    }
    void Initialize() {
        format_.reset(avformat_alloc_context());
        if (!format_) {
            throw std::bad_alloc();
        }
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
        // Demuxers must never open nested paths, playlists or URLs. These
        // whitelists are owned by the format context and freed by its deleter.
        format_->protocol_whitelist = av_strdup("");
        format_->format_whitelist = av_strdup("wav,flac,mp3,aac,ogg,mov,matroska");
        if (!format_->protocol_whitelist || !format_->format_whitelist) {
            throw std::bad_alloc();
        }
        auto* opened = format_.release();
        const int open_result = avformat_open_input(&opened, nullptr, nullptr, nullptr);
        format_.reset(opened);
        detail::Check(open_result, "open audio");
        detail::Check(avformat_find_stream_info(format_.get(), nullptr), "inspect audio");
        stream_ = av_find_best_stream(format_.get(), AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
        detail::Check(stream_, "find audio stream");
        const auto& parameters = *format_->streams[stream_]->codecpar;
        const auto* decoder = avcodec_find_decoder(parameters.codec_id);
        if (!decoder) {
            throw std::runtime_error("audio decoder unavailable");
        }
        codec_.reset(avcodec_alloc_context3(decoder));
        if (!codec_) {
            throw std::bad_alloc();
        }
        detail::Check(avcodec_parameters_to_context(codec_.get(), &parameters), "audio parameters");
        codec_->thread_count = 1;
        codec_->pkt_timebase = format_->streams[stream_]->time_base;
        codec_->max_samples = kMaximumDecodedFrames;
        detail::Check(avcodec_open2(codec_.get(), decoder, nullptr), "open audio decoder");
        if (codec_->sample_rate < 8000 || codec_->sample_rate > 192000 ||
            codec_->ch_layout.nb_channels < 1 || codec_->ch_layout.nb_channels > 8) {
            throw std::runtime_error("audio profile requires 8-192 kHz and 1-8 channels");
        }
        info_.source_sample_rate_ = static_cast<std::uint32_t>(codec_->sample_rate);
        info_.source_channels_ = static_cast<std::uint32_t>(codec_->ch_layout.nb_channels);
        if (format_->duration != AV_NOPTS_VALUE && format_->duration >= 0) {
            info_.duration_seconds_ = static_cast<double>(format_->duration) / AV_TIME_BASE;
        }
        packet_.reset(av_packet_alloc());
        frame_.reset(av_frame_alloc());
        if (!packet_ || !frame_) {
            throw std::bad_alloc();
        }
    }

    AudioInfo Info() const { return info_; }

    std::optional<AudioBlock> Read(std::stop_token stop) {
        if (stop.stop_requested()) {
            throw std::runtime_error("media operation canceled");
        }
        if (failed_) {
            throw std::runtime_error("audio read failed; seek or reopen before continuing");
        }
        try {
            return ReadBlock(stop);
        } catch (...) {
            // A decoder may already have consumed packets when cancellation or
            // an I/O error arrives. Never relabel later samples with an old index.
            failed_ = true;
            throw;
        }
    }

    std::optional<AudioBlock> ReadBlock(std::stop_token stop) {
        input_->SetStop(stop);
        AudioBlock block{};
        block.first_sample_ = next_sample_;
        block.generation_ = generation_;
        block.samples_.reserve(kAudioBlockFrames * kAudioChannels);
        while (block.samples_.size() < kAudioBlockFrames * kAudioChannels) {
            if (stop.stop_requested()) {
                throw std::runtime_error("media operation canceled");
            }
            if (pending_offset_ < pending_.size()) {
                const auto count =
                        std::min(pending_.size() - pending_offset_,
                                 kAudioBlockFrames * kAudioChannels - block.samples_.size());
                block.samples_.insert(block.samples_.end(), pending_.begin() + pending_offset_,
                                      pending_.begin() + pending_offset_ + count);
                pending_offset_ += count;
            } else if (!Fill()) {
                break;
            }
        }
        if (block.samples_.empty()) {
            return std::nullopt;
        }
        next_sample_ += block.samples_.size() / kAudioChannels;
        return block;
    }

    void Skip(std::uint64_t target, std::stop_token stop) {
        while (next_sample_ < target) {
            auto block = Read(stop);
            if (!block) {
                throw std::out_of_range("audio seek is past decoded EOF");
            }
            if (next_sample_ > target) {
                // Restore the unconsumed tail followed by the existing bounded
                // conversion buffer; preserve the exact sample lattice on seek.
                const auto offset =
                        static_cast<std::size_t>(target - block->first_sample_) * kAudioChannels;
                std::vector<float> tail(block->samples_.begin() + offset, block->samples_.end());
                tail.insert(tail.end(), pending_.begin() + pending_offset_, pending_.end());
                pending_ = std::move(tail);
                pending_offset_ = 0;
                next_sample_ = target;
            }
        }
    }

   private:
    int PresentedSamples() const {
        const auto& stream = *format_->streams[stream_];
        // MOV stores AAC end padding as the last packet's shorter duration.
        // FFmpeg 6.1 decodes the complete AAC frame. Honor that exact final
        // packet interval, never a format-wide or bitrate-estimated duration.
        if (codec_->codec_id != AV_CODEC_ID_AAC || !av_match_name("mov", format_->iformat->name) ||
            stream.start_time < 0 || stream.duration <= 0 || frame_->pts < stream.start_time ||
            frame_->duration <= 0)
            return frame_->nb_samples;
        const auto position = frame_->pts - stream.start_time;
        if (position > stream.duration || frame_->duration != stream.duration - position)
            return frame_->nb_samples;
        const AVRational sample_time{1, frame_->sample_rate};
        const auto samples = av_rescale_q(frame_->duration, stream.time_base, sample_time);
        if (samples <= 0 || samples >= frame_->nb_samples ||
            av_compare_ts(samples, sample_time, frame_->duration, stream.time_base) != 0)
            return frame_->nb_samples;
        return static_cast<int>(samples);
    }

    bool Receive() {
        for (;;) {
            if (input_->Canceled()) {
                throw std::runtime_error("media operation canceled");
            }
            const int result = avcodec_receive_frame(codec_.get(), frame_.get());
            if (result == 0) {
                return true;
            }
            if (result == AVERROR_EOF) {
                return false;
            }
            if (result != AVERROR(EAGAIN)) {
                detail::Check(result, "receive audio frame");
            }
            if (sent_eof_) {
                throw std::runtime_error("audio decoder stalled while draining");
            }
            for (;;) {
                av_packet_unref(packet_.get());
                const int read_result = av_read_frame(format_.get(), packet_.get());
                if (read_result == AVERROR_EOF) {
                    detail::Check(avcodec_send_packet(codec_.get(), nullptr),
                                  "drain audio decoder");
                    sent_eof_ = true;
                    break;
                }
                detail::Check(read_result, "read audio packet");
                if (input_->Canceled()) {
                    throw std::runtime_error("media operation canceled");
                }
                if (packet_->stream_index == stream_) {
                    if (packet_->size > 16 * 1024 * 1024) {
                        throw std::runtime_error("audio packet exceeds profile budget");
                    }
                    detail::Check(avcodec_send_packet(codec_.get(), packet_.get()),
                                  "send audio packet");
                    break;
                }
            }
        }
    }

    bool Fill() {
        pending_.clear();
        pending_offset_ = 0;
        if (drained_) {
            return false;
        }
        av_frame_unref(frame_.get());
        const bool received = Receive();
        if (received) {
            if (frame_->nb_samples < 0 || frame_->nb_samples > kMaximumDecodedFrames ||
                frame_->sample_rate != codec_->sample_rate ||
                av_channel_layout_compare(&frame_->ch_layout, &codec_->ch_layout) != 0) {
                throw std::runtime_error("audio frame exceeds profile or changes format");
            }
            if (!resampler_) {
                AVChannelLayout output_layout = AV_CHANNEL_LAYOUT_STEREO;
                SwrContext* created = nullptr;
                const int result = swr_alloc_set_opts2(&created, &output_layout, AV_SAMPLE_FMT_FLT,
                                                       kAudioSampleRate, &frame_->ch_layout,
                                                       static_cast<AVSampleFormat>(frame_->format),
                                                       frame_->sample_rate, 0, nullptr);
                resampler_.reset(created);
                detail::Check(result, "allocate audio conversion");
                detail::Check(swr_init(resampler_.get()), "initialize audio conversion");
                sample_format_ = frame_->format;
            } else if (frame_->format != sample_format_) {
                throw std::runtime_error("audio sample format changed");
            }
        } else if (!resampler_) {
            drained_ = true;
            return false;
        }
        const int input_frames = received ? PresentedSamples() : 0;
        const int capacity = swr_get_out_samples(resampler_.get(), input_frames);
        detail::Check(capacity, "audio conversion capacity");
        if (capacity > kMaximumConvertedFrames) {
            throw std::runtime_error("audio conversion exceeds memory budget");
        }
        pending_.resize(static_cast<std::size_t>(std::max(capacity, 1)) * kAudioChannels);
        auto* output = reinterpret_cast<std::uint8_t*>(pending_.data());
        const int frames = swr_convert(
                resampler_.get(), &output, std::max(capacity, 1),
                received ? const_cast<const std::uint8_t**>(frame_->extended_data) : nullptr,
                input_frames);
        detail::Check(frames, "convert audio");
        pending_.resize(static_cast<std::size_t>(frames) * kAudioChannels);
        for (auto& value : pending_) {
            value = std::isfinite(value) ? std::clamp(value, -1.0F, 1.0F) : 0.0F;
        }
        drained_ = !received && frames == 0;
        return !drained_;
    }

    std::filesystem::path path_{};
    std::shared_ptr<const std::vector<std::uint8_t>> bytes_{};
    storage::FileBytes file_bytes_{};
    std::unique_ptr<detail::LocalInput> input_{};
    std::unique_ptr<AVFormatContext, detail::FormatDelete> format_{};
    std::unique_ptr<AVCodecContext, detail::CodecDelete> codec_{};
    std::unique_ptr<AVPacket, detail::PacketDelete> packet_{};
    std::unique_ptr<AVFrame, detail::FrameDelete> frame_{};
    std::unique_ptr<SwrContext, detail::ResamplerDelete> resampler_{};
    AudioInfo info_{};
    std::vector<float> pending_{};
    std::size_t pending_offset_ = 0;
    std::uint64_t generation_ = 0;
    std::uint64_t next_sample_ = 0;
    int stream_ = -1;
    int sample_format_ = -1;
    bool sent_eof_ = false;
    bool drained_ = false;
    bool failed_ = false;
};

AudioDecoder::AudioDecoder(const std::filesystem::path& path, std::uint64_t generation,
                           std::stop_token stop)
    : impl_(std::make_unique<Impl>(path, generation, stop)) {}
AudioDecoder::AudioDecoder(std::shared_ptr<const std::vector<std::uint8_t>> bytes,
                           std::uint64_t generation, std::stop_token stop)
    : impl_(std::make_unique<Impl>(std::move(bytes), generation, stop)) {}
AudioDecoder::~AudioDecoder() = default;
AudioDecoder::AudioDecoder(storage::FileBytes bytes, std::uint64_t generation, std::stop_token stop)
    : impl_(std::make_unique<Impl>(std::move(bytes), generation, stop)) {}
AudioDecoder::AudioDecoder(AudioDecoder&&) noexcept = default;
AudioDecoder& AudioDecoder::operator=(AudioDecoder&&) noexcept = default;
AudioInfo AudioDecoder::Info() const { return impl_->Info(); }
std::optional<AudioBlock> AudioDecoder::Read(std::stop_token stop) { return impl_->Read(stop); }
void AudioDecoder::Seek(std::uint64_t first_sample, std::uint64_t generation,
                        std::stop_token stop) {
    auto replacement = impl_->Reopen(generation, stop);
    replacement->Skip(first_sample, stop);
    impl_ = std::move(replacement);
}
}  // namespace rhythm::media
