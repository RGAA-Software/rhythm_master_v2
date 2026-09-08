#include "stream_source.h"

#include <stdexcept>
#include <type_traits>

namespace rhythm::audio::detail {
void ValidateSource(const PlaybackSource& source) {
    const bool valid = std::visit(
            [](const auto& value) {
                using Type = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<Type, std::monostate>)
                    return false;
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
AudioStream::AudioStream(const PlaybackSource& source, std::uint64_t generation,
                         std::stop_token stop) {
    ValidateSource(source);
    std::visit(
            [&](const auto& value) {
                using Type = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<Type, std::monostate>)
                    throw std::invalid_argument("audio.playback_source");
                else if constexpr (std::is_same_v<
                                           Type,
                                           std::shared_ptr<const media::AudioArrangementSource>>)
                    mixer_ = std::make_unique<media::AudioMixer>(*value, generation);
                else if constexpr (std::is_same_v<
                                           Type,
                                           std::shared_ptr<const media::AudioArrangementFiles>>)
                    mixer_ = std::make_unique<media::AudioMixer>(*value, generation, stop);
                else
                    decoder_ = std::make_unique<media::AudioDecoder>(value, generation, stop);
            },
            source);
}
media::AudioInfo AudioStream::Info() const { return mixer_ ? mixer_->Info() : decoder_->Info(); }
std::optional<media::AudioBlock> AudioStream::Read(std::stop_token stop) {
    return mixer_ ? mixer_->Read(stop) : decoder_->Read(stop);
}
void AudioStream::Seek(std::uint64_t sample, std::uint64_t generation, std::stop_token stop) {
    if (mixer_)
        mixer_->Seek(sample, generation, stop);
    else
        decoder_->Seek(sample, generation, stop);
}
}  // namespace rhythm::audio::detail
