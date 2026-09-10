#include "rhythm/runtime/playback_clock.h"

#include <cmath>
#include <stdexcept>

namespace rhythm::runtime {
double PlaybackClock::Advance(double host_seconds, bool suspended,
                              const std::optional<PlaybackSample>& source) {
    if (source &&
        (!std::isfinite(source->seconds_) || source->seconds_ < 0 ||
         (source->duration_ && (!std::isfinite(*source->duration_) || *source->duration_ <= 0)) ||
         (source->continuous_ &&
          (!std::isfinite(source->continuous_->seconds_) || source->continuous_->seconds_ < 0))))
        throw std::invalid_argument("clock.media_time");
    const auto previous_seconds = clock_.Seconds();
    // Validate the monotonic host input before changing transport state. While
    // following a source, retain the host anchor for a smooth local fallback.
    clock_.Advance(host_seconds, suspended || paused_ || source.has_value());
    if (suspended) return clock_.Seconds();
    if (source) {
        const auto motion_source =
                source->continuous_.value_or(MotionTime{source->seconds_, source->generation_});
        if (!motion_source_ || motion_source_->generation_ != motion_source.generation_ ||
            motion_source.seconds_ < motion_source_->seconds_) {
            motion_ = {source->seconds_, motion_.generation_ + 1};
        } else if (!source->paused_) {
            motion_.seconds_ += motion_source.seconds_ - motion_source_->seconds_;
        }
        motion_source_ = motion_source;
        if (source_generation_ != source->generation_ || source->seconds_ < clock_.Seconds())
            ++generation_;
        source_generation_ = source->generation_;
        clock_.Seek(source->seconds_);
        paused_ = source->paused_;
    } else if (source_generation_) {
        source_generation_.reset();
        motion_source_.reset();
        ++generation_;
    } else {
        motion_.seconds_ += clock_.Seconds() - previous_seconds;
    }
    return clock_.Seconds();
}
void PlaybackClock::Seek(double seconds) {
    clock_.Seek(seconds);
    ++generation_;
    motion_ = {seconds, motion_.generation_ + 1};
    motion_source_.reset();
}
void PlaybackClock::Loop(double seconds) {
    clock_.Seek(seconds);
    ++generation_;
}
}  // namespace rhythm::runtime
