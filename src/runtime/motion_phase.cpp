#include "rhythm/runtime/motion_phase.h"

#include <cmath>
#include <stdexcept>

namespace rhythm::runtime {
namespace {
double Wrap(double value, double period) {
    const auto wrapped = std::fmod(value, period);
    return wrapped < 0 ? wrapped + period : wrapped;
}
double Distance(double seconds, double rate, double period) {
    if (rate == 0) return 0;
    // Reduce elapsed time before multiplying, including after a very long gap.
    return Wrap(std::fmod(seconds, period / std::abs(rate)) * rate, period);
}
}  // namespace
double MotionPhase::Advance(double seconds, double rate, double period, bool advance) {
    if (!std::isfinite(seconds) || seconds < 0 || !std::isfinite(rate) || std::abs(rate) > 1e6 ||
        !std::isfinite(period) || period < 1e-6 || period > 1e6)
        throw std::invalid_argument("motion_phase.input");
    const bool restart = !previous_ || seconds < *previous_ || period != period_;
    if (restart) {
        phase_ = Distance(seconds, rate, period);
    } else if (advance) {
        // Anchor constant-rate spans instead of summing one rounded delta per
        // rendered frame. Cycle seams must not drift with frame rate.
        phase_ = Wrap(anchor_phase_ + Distance(seconds - anchor_seconds_, rate_, period), period);
    }
    if (restart || !advance || rate != rate_) {
        anchor_seconds_ = seconds;
        anchor_phase_ = phase_;
    }
    previous_ = seconds;
    rate_ = rate;
    period_ = period;
    return phase_;
}
}  // namespace rhythm::runtime
