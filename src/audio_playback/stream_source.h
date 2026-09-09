#pragma once

#include <variant>

#include "rhythm/media/audio_mixer.h"

namespace rhythm::audio::detail {
// An explicit empty old bus when a scene has no selected soundtrack. It emits
// bounded PCM on the same device clock; not a decoder fallback for broken media.
struct SilentSource {};
using PlaybackSource =
        std::variant<std::monostate, std::filesystem::path,
                     std::shared_ptr<const std::vector<std::uint8_t>>, storage::FileBytes,
                     std::shared_ptr<const media::AudioArrangementSource>,
                     std::shared_ptr<const media::AudioArrangementFiles>, SilentSource>;
// Value validation only; file opening and source probing stay on the worker.
void ValidateSource(const PlaybackSource& source);
// Conservative peak over the entire arrangement, excluding silent clips.
// Used before admitting a second scene, including clips that start later.
std::size_t RequiredCursors(const PlaybackSource& source);
// Owns exactly one PCM cursor chosen by the source kind. Device, analysis,
// scheduling and transport intent remain in FilePlayback.
class AudioStream final {
   public:
    AudioStream(const PlaybackSource& source, std::uint64_t generation, std::stop_token stop,
                media::AudioCursorBudget budget = {});
    media::AudioInfo Info() const;
    std::optional<media::AudioBlock> Read(std::stop_token stop);
    void Seek(std::uint64_t sample, std::uint64_t generation, std::stop_token stop);

   private:
    media::AudioCursorBudget::Lease lease_{};
    std::unique_ptr<media::AudioDecoder> decoder_{};
    std::unique_ptr<media::AudioMixer> mixer_{};
    bool silent_ = false;
    std::uint64_t silent_sample_ = 0;
    std::uint64_t silent_generation_ = 0;
};
}  // namespace rhythm::audio::detail
