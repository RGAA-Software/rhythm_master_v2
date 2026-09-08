#pragma once

#include <memory>
#include <vector>

#include "rhythm/assets/types.h"
#include "rhythm/parameters/clip_interval.h"
#include "rhythm/storage/file_bytes.h"

namespace rhythm::media {
struct AudioClip {
    std::uint64_t id_ = 0;
    std::string title_{};
    assets::AssetId asset_{};
    parameters::ClipTiming timing_{};
    float gain_ = 1;
    // Stereo balance: center preserves channels, -1 silences right, +1 left.
    float pan_ = 0;
    bool muted_ = false;
    bool operator==(const AudioClip&) const = default;
};
struct AudioClipSamples {
    std::uint64_t start_ = 0;
    std::uint64_t duration_ = 0;
    std::uint64_t source_in_ = 0;
    std::uint64_t source_out_ = 0;
    bool operator==(const AudioClipSamples&) const = default;
};
// Immutable, sample-aligned schedule, not a playback service. Native-rate audio
// avoids pitch changes when editing placement. Blank and source looping are
// supported; holding a sample would introduce DC and is intentionally invalid.
class AudioArrangement final {
   public:
    AudioArrangement() = default;
    explicit AudioArrangement(std::vector<AudioClip> clips);
    const std::vector<AudioClip>& Clips() const { return clips_; }
    const std::vector<AudioClipSamples>& Samples() const { return samples_; }
    std::uint64_t DurationSamples() const { return duration_; }
    bool operator==(const AudioArrangement&) const = default;
    static constexpr std::uint32_t kSampleRate = 48000;
    static constexpr std::size_t kMaximumClips = 32;
    static constexpr std::size_t kMaximumConcurrent = 4;

   private:
    std::vector<AudioClip> clips_{};
    std::vector<AudioClipSamples> samples_{};
    std::uint64_t duration_ = 0;
};
struct AudioAssetSource {
    assets::AssetId id_{};
    // Exactly one verified immutable input. File leases are read by workers.
    std::shared_ptr<const std::vector<std::uint8_t>> bytes_{};
    storage::FileBytes file_bytes_{};
};
struct AudioArrangementSource {
    AudioArrangement arrangement_{};
    std::vector<AudioAssetSource> assets_{};
};
}  // namespace rhythm::media
