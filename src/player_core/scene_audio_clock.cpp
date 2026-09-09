#include "scene_audio_clock.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace rhythm::player {
namespace {
bool ValidPosition(const std::optional<SceneAudioPosition>& position) {
    return !position || (std::isfinite(position->seconds_) && position->seconds_ >= 0);
}
void ObserveIteration(std::optional<std::uint64_t>& previous, std::uint64_t incoming,
                      std::uint64_t& generation) {
    if (previous && *previous != incoming) ++generation;
    previous = incoming;
}
}  // namespace
void SceneAudioClock::Begin(std::uint64_t id, double duration, std::uint64_t generation) {
    if (!id || !generation || !std::isfinite(duration) || duration < 0 || duration > 5 || Active())
        throw std::invalid_argument("player.audio_transition_clock");
    id_ = id;
    duration_ = duration;
    previous_generation_ = incoming_generation_ = generation;
}
SceneAudioFrame SceneAudioClock::Step(const runtime::PlaybackSample& fallback,
                                      const std::optional<SceneAudioSample>& sample) {
    SceneAudioFrame frame{fallback, {0, incoming_generation_, true, {}}};
    if (!Active()) return frame;
    if (!sample || sample->transition_id_ != id_) {
        if (std::isfinite(fallback.seconds_) && fallback.seconds_ >= 0) previous_sample_ = fallback;
        return frame;
    }
    if (!std::isfinite(fallback.seconds_) || fallback.seconds_ < 0 ||
        !std::isfinite(sample->elapsed_seconds_) || sample->elapsed_seconds_ < 0 ||
        sample->elapsed_seconds_ > 5 || !ValidPosition(sample->previous_) ||
        !ValidPosition(sample->incoming_) || sample->phase_ < SceneAudioPhase::kWaiting ||
        sample->phase_ > SceneAudioPhase::kFailed ||
        (sample->phase_ == SceneAudioPhase::kCommitted && !sample->incoming_))
        throw std::invalid_argument("player.audio_transition_sample");
    if (sample->previous_) {
        if (!previous_offset_) previous_offset_ = fallback.seconds_ - sample->previous_->seconds_;
        ObserveIteration(previous_iteration_, sample->previous_->iteration_, previous_generation_);
        frame.previous_ = {std::max(0.0, sample->previous_->seconds_ + *previous_offset_),
                           previous_generation_,
                           sample->paused_,
                           {}};
        previous_sample_ = frame.previous_;
    } else if (sample->incoming_ && previous_sample_) {
        // A serial cut can queue new PCM while holding only the old recovery
        // checkpoint. Its new primary clock must not animate/rewind the old work.
        frame.previous_ = *previous_sample_;
        frame.previous_.paused_ = true;
    }
    frame.running_ = sample->phase_ == SceneAudioPhase::kRunning;
    frame.committed_ = sample->phase_ == SceneAudioPhase::kCommitted;
    frame.abort_ = sample->phase_ == SceneAudioPhase::kRecovering ||
                   sample->phase_ == SceneAudioPhase::kCanceled ||
                   sample->phase_ == SceneAudioPhase::kFailed;
    frame.terminal_ = frame.committed_ || sample->phase_ == SceneAudioPhase::kCanceled ||
                      sample->phase_ == SceneAudioPhase::kFailed;
    if (sample->incoming_) {
        ObserveIteration(incoming_iteration_, sample->incoming_->iteration_, incoming_generation_);
        frame.incoming_ = {sample->incoming_->seconds_,
                           incoming_generation_,
                           sample->paused_ || (!frame.running_ && !frame.committed_),
                           {}};
    }
    if (frame.committed_)
        frame.progress_ = 1;
    else if (frame.running_)
        frame.progress_ =
                duration_ > 0 ? std::clamp(sample->elapsed_seconds_ / duration_, 0.0, 1.0) : 0;
    return frame;
}
}  // namespace rhythm::player
