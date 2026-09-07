#include "rhythm/runtime/playback_clock.h"

#include <cmath>
#include <stdexcept>

namespace rhythm::runtime {
double PlaybackClock::Advance(double host_seconds, bool suspended,
                              const std::optional<PlaybackSample>& source) {
    if (source &&
        (!std::isfinite(source->seconds_) || source->seconds_ < 0 ||
         (source->duration_ && (!std::isfinite(*source->duration_) || *source->duration_ <= 0))))
        throw std::invalid_argument("clock.media_time");
    // Validate the monotonic host input before changing transport state. While
    // following a source, retain the host anchor for a smooth local fallback.
    clock_.Advance(host_seconds, suspended || paused_ || source.has_value());
    if (suspended) return clock_.Seconds();
    if (source) {
        if (source_generation_ != source->generation_ || source->seconds_ < clock_.Seconds())
            ++generation_;
        source_generation_ = source->generation_;
        clock_.Seek(source->seconds_);
        paused_ = source->paused_;
    } else if (source_generation_) {
        source_generation_.reset();
        ++generation_;
    }
    return clock_.Seconds();
}
void PlaybackClock::Seek(double seconds) {
    clock_.Seek(seconds);
    ++generation_;
}
}  // namespace rhythm::runtime
