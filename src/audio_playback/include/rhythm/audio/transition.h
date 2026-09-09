#pragma once

#include <cstdint>
#include <string>

namespace rhythm::audio {
enum class TransitionCurve { kLinear, kEqualPower };
enum class AudioTransitionState {
    kNone,
    kPreparing,
    kQueued,
    kMixing,
    kRecovering,
    kCompleted,
    kCanceled,
    kFailed
};
struct AudioTransitionSnapshot {
    std::uint64_t id_ = 0;
    AudioTransitionState state_ = AudioTransitionState::kNone;
    std::uint64_t start_frame_ = 0;
    std::uint64_t end_frame_ = 0;
    std::uint64_t elapsed_frames_ = 0;
    std::uint64_t duration_frames_ = 0;
    double incoming_seconds_ = 0;
    std::uint64_t incoming_iteration_ = 0;
    bool incoming_presented_ = false;
    std::uint64_t clipped_samples_ = 0;
    std::string error_{};
};
inline bool AudioTransitionActive(AudioTransitionState state) {
    return state == AudioTransitionState::kPreparing || state == AudioTransitionState::kQueued ||
           state == AudioTransitionState::kMixing || state == AudioTransitionState::kRecovering;
}
}  // namespace rhythm::audio
