#include "stream_source.h"

#include <algorithm>
#include <stdexcept>
#include <type_traits>

namespace rhythm::audio::detail {
void ValidateSource(const PlaybackSource& source) {
    const bool valid = std::visit(
            [](const auto& value) {
                using Type = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<Type, std::monostate>)
                    return false;
                else if constexpr (std::is_same_v<Type, SilentSource>)
                    return true;
                else if constexpr (std::is_same_v<Type, std::filesystem::path>)
                    return !value.empty();
                else if constexpr (std::is_same_v<Type, storage::FileBytes>)
                    return value.Valid() && value.Size();
                else if constexpr (std::is_same_v<Type,
                                                  std::shared_ptr<const std::vector<std::uint8_t>>>)
                    return value && !value->empty() && value->size() <= 16 * 1024 * 1024;
                else
                    return value && !value->arrangement_.Clips().empty();
            },
            source);
    if (!valid) throw std::invalid_argument("audio.playback_source");
}
std::size_t RequiredCursors(const PlaybackSource& source) {
    ValidateSource(source);
    return std::visit(
            [](const auto& value) -> std::size_t {
                using Type = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<
                                      Type, std::shared_ptr<const media::AudioArrangementSource>> ||
                              std::is_same_v<Type,
                                             std::shared_ptr<const media::AudioArrangementFiles>>) {
                    std::vector<std::pair<std::uint64_t, int>> edges;
                    const auto& clips = value->arrangement_.Clips();
                    const auto& samples = value->arrangement_.Samples();
                    for (std::size_t index = 0; index < clips.size(); ++index) {
                        if (clips[index].muted_ || clips[index].gain_ == 0) continue;
                        edges.emplace_back(samples[index].start_, 1);
                        edges.emplace_back(samples[index].start_ + samples[index].duration_, -1);
                    }
                    std::sort(edges.begin(),
                              edges.end());  // Releases before arrivals at equal samples.
                    int active = 0;
                    int peak = 0;
                    for (const auto& edge : edges) {
                        active += edge.second;
                        peak = std::max(peak, active);
                    }
                    return static_cast<std::size_t>(peak);
                } else if constexpr (std::is_same_v<Type, SilentSource>)
                    return 0;
                else
                    return 1;
            },
            source);
}
AudioStream::AudioStream(const PlaybackSource& source, std::uint64_t generation,
                         std::stop_token stop, media::AudioCursorBudget budget) {
    ValidateSource(source);
    std::visit(
            [&](const auto& value) {
                using Type = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<Type, std::monostate>)
                    throw std::invalid_argument("audio.playback_source");
                else if constexpr (std::is_same_v<Type, SilentSource>) {
                    silent_ = true;
                    silent_generation_ = generation;
                } else if constexpr (std::is_same_v<
                                             Type,
                                             std::shared_ptr<const media::AudioArrangementSource>>)
                    mixer_ = std::make_unique<media::AudioMixer>(*value, generation, budget);
                else if constexpr (std::is_same_v<
                                           Type,
                                           std::shared_ptr<const media::AudioArrangementFiles>>)
                    mixer_ = std::make_unique<media::AudioMixer>(*value, generation, stop, budget);
                else {
                    lease_ = budget.Acquire();
                    decoder_ = std::make_unique<media::AudioDecoder>(value, generation, stop);
                }
            },
            source);
}
media::AudioInfo AudioStream::Info() const {
    if (silent_) return {media::kAudioSampleRate, media::kAudioChannels, {}};
    return mixer_ ? mixer_->Info() : decoder_->Info();
}
std::optional<media::AudioBlock> AudioStream::Read(std::stop_token stop) {
    if (silent_) {
        if (stop.stop_requested()) throw std::runtime_error("audio.canceled");
        media::AudioBlock block{
                std::vector<float>(media::kAudioBlockFrames * media::kAudioChannels),
                silent_sample_, silent_generation_};
        silent_sample_ += media::kAudioBlockFrames;
        return block;
    }
    return mixer_ ? mixer_->Read(stop) : decoder_->Read(stop);
}
void AudioStream::Seek(std::uint64_t sample, std::uint64_t generation, std::stop_token stop) {
    if (silent_) {
        if (stop.stop_requested()) throw std::runtime_error("audio.canceled");
        silent_sample_ = sample;
        silent_generation_ = generation;
    } else if (mixer_)
        mixer_->Seek(sample, generation, stop);
    else
        decoder_->Seek(sample, generation, stop);
}
}  // namespace rhythm::audio::detail
