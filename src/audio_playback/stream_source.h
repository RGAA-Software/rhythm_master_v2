#pragma once

#include <variant>

#include "rhythm/media/audio_mixer.h"

namespace rhythm::audio::detail {
using PlaybackSource =
        std::variant<std::monostate, std::filesystem::path,
                     std::shared_ptr<const std::vector<std::uint8_t>>, storage::FileBytes,
                     std::shared_ptr<const media::AudioArrangementSource>,
                     std::shared_ptr<const media::AudioArrangementFiles>>;
// Value validation only; file opening and source probing stay on the worker.
void ValidateSource(const PlaybackSource& source);
// Owns exactly one PCM cursor chosen by the source kind. Device, analysis,
// scheduling and transport intent remain in FilePlayback.
class AudioStream final {
   public:
    AudioStream(const PlaybackSource& source, std::uint64_t generation, std::stop_token stop);
    media::AudioInfo Info() const;
    std::optional<media::AudioBlock> Read(std::stop_token stop);
    void Seek(std::uint64_t sample, std::uint64_t generation, std::stop_token stop);

   private:
    std::unique_ptr<media::AudioDecoder> decoder_{};
    std::unique_ptr<media::AudioMixer> mixer_{};
};
}  // namespace rhythm::audio::detail
