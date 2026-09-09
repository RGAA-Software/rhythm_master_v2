#pragma once

#include "rhythm/player/scene_audio.h"
#include "rhythm/runtime/playback_clock.h"

namespace rhythm::player {
struct SceneAudioFrame {
    runtime::PlaybackSample previous_{};
    runtime::PlaybackSample incoming_{};
    double progress_ = 0;
    bool running_ = false;
    bool committed_ = false;
    bool abort_ = false;
    bool terminal_ = false;
};
// Derives two scene-local clocks from one consumed-audio snapshot. Keeps the
// old visual origin when a silent bus begins at zero. Holds that origin through
// queued recovery even after the incoming GPU session has been discarded.
class SceneAudioClock final {
   public:
    void Begin(std::uint64_t id, double duration, std::uint64_t generation);
    SceneAudioFrame Step(const runtime::PlaybackSample& fallback,
                         const std::optional<SceneAudioSample>& sample);
    void Reset() { *this = {}; }
    bool Active() const { return id_ != 0; }
    std::uint64_t Id() const { return id_; }
    std::optional<double> PreviousOffset() const { return previous_offset_; }

   private:
    std::uint64_t id_ = 0;
    double duration_ = 0;
    std::optional<double> previous_offset_{};
    std::optional<runtime::PlaybackSample> previous_sample_{};
    std::optional<std::uint64_t> previous_iteration_{};
    std::optional<std::uint64_t> incoming_iteration_{};
    std::uint64_t previous_generation_ = 0;
    std::uint64_t incoming_generation_ = 0;
};
}  // namespace rhythm::player
