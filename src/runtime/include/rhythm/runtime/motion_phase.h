#pragma once

#include <optional>

namespace rhythm::runtime {
// Host-thread confined, bounded motion phase. Rate samples take effect from
// their observation time onward (zero-order hold). Repeated times can change
// the rate without moving the phase. Supply playback time, never wall time.
// Fresh history or a backward seek seeds seconds * rate modulo period; it does
// not reconstruct earlier live controls. Reset this value on transport seek.
class MotionPhase final {
   public:
    double Advance(double seconds, double rate, double period, bool advance);

   private:
    std::optional<double> previous_{};
    double rate_ = 1;
    double period_ = 16;
    double phase_ = 0;
    double anchor_seconds_ = 0;
    double anchor_phase_ = 0;
};
}  // namespace rhythm::runtime
