#include "rhythm/runtime/frame_clock.h"

#include <cmath>
#include <stdexcept>

namespace rhythm::runtime {
double FrameClock::Advance(double monotonic_seconds, bool suspended) {
    if (!std::isfinite(monotonic_seconds) || monotonic_seconds < 0 ||
        (previous_ && monotonic_seconds < *previous_))
        throw std::invalid_argument("clock.monotonic_time");
    if (previous_ && !suspended_ && !suspended) seconds_ += monotonic_seconds - *previous_;
    previous_ = monotonic_seconds;
    suspended_ = suspended;
    return seconds_;
}
void FrameClock::Seek(double seconds) {
    if (!std::isfinite(seconds) || seconds < 0) throw std::invalid_argument("clock.seek_time");
    seconds_ = seconds;
}
}  // namespace rhythm::runtime
