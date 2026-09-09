#include "rhythm/media/audio_mixer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <stdexcept>

namespace rhythm::media {
namespace {
AudioArrangementSource OpenFiles(AudioArrangementFiles files, std::stop_token stop) {
    if (files.files_.empty() || files.files_.size() > AudioArrangement::kMaximumClips)
        throw std::invalid_argument("audio.arrangement_source");
    AudioArrangementSource source{std::move(files.arrangement_), {}};
    for (const auto& file : files.files_) {
        if (stop.stop_requested()) throw std::runtime_error("audio.canceled");
        source.assets_.push_back(
                {file.id_, {}, storage::FileBytes::Open(file.path_, 256 * 1024 * 1024)});
    }
    return source;
}
}  // namespace
static_assert(AudioArrangement::kSampleRate == kAudioSampleRate);
class AudioMixer::Impl final {
   public:
    explicit Impl(AudioArrangementSource source, std::uint64_t generation, AudioCursorBudget budget)
        : source_(std::move(source)), budget_(std::move(budget)), generation_(generation) {
        if (source_.arrangement_.Clips().empty() || source_.assets_.empty() ||
            source_.assets_.size() > AudioArrangement::kMaximumClips)
            throw std::invalid_argument("audio.arrangement_source");
        std::map<std::string, std::size_t> indices;
        for (std::size_t index = 0; index < source_.assets_.size(); ++index) {
            const auto& asset = source_.assets_[index];
            if (!assets::ValidId(asset.id_) || !indices.emplace(asset.id_.sha256_, index).second ||
                bool(asset.bytes_) == asset.file_bytes_.Valid() ||
                (asset.bytes_ &&
                 (asset.bytes_->empty() || asset.bytes_->size() > 16 * 1024 * 1024)) ||
                (asset.file_bytes_.Valid() &&
                 (!asset.file_bytes_.Size() || asset.file_bytes_.Size() > 256 * 1024 * 1024)))
                throw std::invalid_argument("audio.arrangement_source");
        }
        for (const auto& clip : source_.arrangement_.Clips()) {
            const auto found = indices.find(clip.asset_.sha256_);
            if (found == indices.end()) throw std::invalid_argument("audio.arrangement_source");
            asset_indices_.push_back(found->second);
        }
        entries_.resize(asset_indices_.size());
        for (std::size_t index = 0; index < entries_.size(); ++index) order_.push_back(index);
        const auto& timing = source_.arrangement_.Samples();
        // Finish and release short clips before opening later clips in the same
        // PCM block. Non-overlapping tracks must not accumulate open decoders.
        std::stable_sort(order_.begin(), order_.end(), [&](auto a, auto b) {
            return timing[a].start_ + timing[a].duration_ < timing[b].start_ + timing[b].duration_;
        });
    }
    AudioInfo Info() const {
        return {kAudioSampleRate, kAudioChannels,
                double(source_.arrangement_.DurationSamples()) / kAudioSampleRate};
    }
    void Seek(std::uint64_t sample, std::uint64_t generation, std::stop_token stop) {
        if (stop.stop_requested()) throw std::runtime_error("audio.canceled");
        if (sample > static_cast<std::uint64_t>(parameters::ClipInterval::kMaximumSeconds *
                                                kAudioSampleRate))
            throw std::invalid_argument("audio.seek_range");
        for (auto& entry : entries_) entry = {};
        sample_ = sample;
        generation_ = generation;
        failed_ = false;
    }
    std::optional<AudioBlock> Read(std::stop_token stop) {
        if (stop.stop_requested()) throw std::runtime_error("audio.canceled");
        if (failed_) throw std::runtime_error("audio.mixer_requires_seek");
        const auto duration = source_.arrangement_.DurationSamples();
        if (sample_ >= duration) return {};
        const auto frames = std::min<std::uint64_t>(kAudioBlockFrames, duration - sample_);
        std::vector<double> sum(static_cast<std::size_t>(frames) * kAudioChannels);
        try {
            const auto& clips = source_.arrangement_.Clips();
            const auto& aligned = source_.arrangement_.Samples();
            for (const auto index : order_) {
                if (stop.stop_requested()) throw std::runtime_error("audio.canceled");
                const auto& clip = clips[index];
                const auto& timing = aligned[index];
                const auto begin = std::max(sample_, timing.start_);
                const auto end = std::min(sample_ + frames, timing.start_ + timing.duration_);
                if (!clip.muted_ && clip.gain_ > 0) {
                    const std::array<double, 2> balance{1 - std::max(0.0F, clip.pan_),
                                                        1 + std::min(0.0F, clip.pan_)};
                    for (auto frame = begin; frame < end; ++frame) {
                        const auto local = frame - timing.start_;
                        const auto source =
                                timing.source_in_ +
                                (clip.timing_.end_ == parameters::ClipEnd::kLoop
                                         ? local % (timing.source_out_ - timing.source_in_)
                                         : local);
                        const auto input = Sample(index, source, stop);
                        const auto gain =
                                clip.gain_ * parameters::EnvelopeGain(
                                                     double(local) / kAudioSampleRate,
                                                     double(timing.duration_) / kAudioSampleRate,
                                                     clip.timing_.fade_in_, clip.timing_.fade_out_,
                                                     clip.timing_.smooth_);
                        for (std::size_t channel = 0; channel < kAudioChannels; ++channel) {
                            if (!std::isfinite(input[channel]))
                                throw std::invalid_argument("audio.invalid_sample");
                            sum[static_cast<std::size_t>(frame - sample_) * kAudioChannels +
                                channel] += input[channel] * gain * balance[channel];
                        }
                    }
                }
                if (clip.muted_ || clip.gain_ == 0 ||
                    timing.start_ + timing.duration_ <= sample_ + frames)
                    entries_[index] = {};
            }
            AudioBlock result{{}, sample_, generation_};
            result.samples_.reserve(sum.size());
            for (const auto value : sum)
                result.samples_.push_back(static_cast<float>(std::clamp(value, -1.0, 1.0)));
            sample_ += frames;
            return result;
        } catch (...) {
            failed_ = true;
            throw;
        }
    }

   private:
    struct Entry {
        AudioCursorBudget::Lease lease_{};
        std::unique_ptr<AudioDecoder> decoder_{};
        std::optional<AudioBlock> block_{};
    };
    std::array<float, 2> Sample(std::size_t index, std::uint64_t sample, std::stop_token stop) {
        auto& entry = entries_[index];
        const auto& asset = source_.assets_[asset_indices_[index]];
        if (!entry.decoder_) {
            auto lease = budget_.Acquire();
            auto decoder =
                    asset.bytes_
                            ? std::make_unique<AudioDecoder>(asset.bytes_, generation_, stop)
                            : std::make_unique<AudioDecoder>(asset.file_bytes_, generation_, stop);
            const auto info = decoder->Info();
            const auto source_out = source_.arrangement_.Samples()[index].source_out_;
            if (info.duration_seconds_ && std::isfinite(*info.duration_seconds_) &&
                double(source_out) / kAudioSampleRate >
                        *info.duration_seconds_ + 1.0 / kAudioSampleRate)
                throw std::invalid_argument("audio.clip_source_range");
            if (sample) decoder->Seek(sample, generation_, stop);
            entry.decoder_ = std::move(decoder);
            entry.lease_ = std::move(lease);
        }
        if (entry.block_ && (sample < entry.block_->first_sample_ ||
                             sample > entry.block_->first_sample_ +
                                              entry.block_->samples_.size() / kAudioChannels)) {
            entry.decoder_->Seek(sample, generation_, stop);
            entry.block_.reset();
        }
        if (!entry.block_ ||
            sample == entry.block_->first_sample_ + entry.block_->samples_.size() / kAudioChannels)
            entry.block_ = entry.decoder_->Read(stop);
        if (!entry.block_ || sample < entry.block_->first_sample_ ||
            sample >= entry.block_->first_sample_ + entry.block_->samples_.size() / kAudioChannels)
            throw std::invalid_argument("audio.clip_source_range");
        const auto offset =
                static_cast<std::size_t>(sample - entry.block_->first_sample_) * kAudioChannels;
        return {entry.block_->samples_[offset], entry.block_->samples_[offset + 1]};
    }
    AudioArrangementSource source_{};
    AudioCursorBudget budget_{};
    std::vector<std::size_t> asset_indices_{};
    std::vector<std::size_t> order_{};
    std::vector<Entry> entries_{};
    std::uint64_t sample_ = 0;
    std::uint64_t generation_ = 1;
    bool failed_ = false;
};
AudioMixer::AudioMixer(AudioArrangementSource source, std::uint64_t generation,
                       AudioCursorBudget budget)
    : impl_(std::make_unique<Impl>(std::move(source), generation, std::move(budget))) {}
AudioMixer::AudioMixer(AudioArrangementFiles files, std::uint64_t generation, std::stop_token stop,
                       AudioCursorBudget budget)
    : AudioMixer(OpenFiles(std::move(files), stop), generation, std::move(budget)) {}
AudioMixer::~AudioMixer() = default;
AudioMixer::AudioMixer(AudioMixer&&) noexcept = default;
AudioMixer& AudioMixer::operator=(AudioMixer&&) noexcept = default;
AudioInfo AudioMixer::Info() const { return impl_->Info(); }
std::optional<AudioBlock> AudioMixer::Read(std::stop_token stop) { return impl_->Read(stop); }
void AudioMixer::Seek(std::uint64_t sample, std::uint64_t generation, std::stop_token stop) {
    impl_->Seek(sample, generation, stop);
}
}  // namespace rhythm::media
