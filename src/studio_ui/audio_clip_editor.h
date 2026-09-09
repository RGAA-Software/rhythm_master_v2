#pragma once

#include <map>
#include <memory>
#include <optional>

#include "rhythm/media/audio_arrangement.h"

namespace rhythm::media {
class WaveformIndex;
}

namespace rhythm::studio {
using ClipWaveforms = std::map<std::string, std::shared_ptr<const media::WaveformIndex>>;
struct AudioClipEdit {
    std::optional<std::vector<media::AudioClip>> clips_{};
    bool committed_ = false;
};
// UI-thread selection and gesture state. Reuses the time-section bar interaction;
// validates the entire mix before publishing a draft. Owns no media or clock.
class AudioClipEditor final {
   public:
    AudioClipEdit Draw(const std::vector<media::AudioClip>& clips, double playhead, double duration,
                       double bpm, const std::map<std::string, std::string>& text,
                       const ClipWaveforms& waveforms = {});
    void Reset();

   private:
    struct Drag {
        media::AudioClip origin_{};
        float mouse_x_ = 0;
        int mode_ = 0;
        bool changed_ = false;
    };
    std::optional<Drag> drag_{};
    std::uint64_t selected_ = 0;
    bool property_active_ = false;
    bool snap_ = true;
    std::string error_{};
};
}  // namespace rhythm::studio
