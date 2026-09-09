#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace rhythm::player {
enum class SceneAudioPhase { kWaiting, kRunning, kRecovering, kCommitted, kCanceled, kFailed };
struct SceneAudioPosition {
    double seconds_ = 0;
    std::uint64_t iteration_ = 0;
};
// Host translates its audio service into value snapshots. No device, decoder
// or audio service types enter the scene/session contract. IDs identify THIS
// scene transition; source-local loop counters are independent for both works.
struct SceneAudioSample {
    std::uint64_t transition_id_ = 0;
    SceneAudioPhase phase_ = SceneAudioPhase::kWaiting;
    bool paused_ = false;
    double elapsed_seconds_ = 0;
    std::optional<SceneAudioPosition> previous_{};
    std::optional<SceneAudioPosition> incoming_{};
    std::string error_{};
};
}  // namespace rhythm::player
