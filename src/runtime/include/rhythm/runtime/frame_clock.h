#pragma once

#include <optional>

namespace rhythm::runtime {
// Local playback time driven by host monotonic seconds. Suspended intervals do
// not enter graph time, and resuming never produces a catch-up delta. This value
// clock has no platform, audio-device or decoder dependency.
class FrameClock final {
   public:
    double Advance(double monotonic_seconds, bool suspended);
    // Reposition at the most recently observed host time. The next tick keeps
    // its elapsed interval; monotonic ordering checks survive the seek.
    void Seek(double seconds);
    [[nodiscard]] double Seconds() const { return seconds_; }

   private:
    std::optional<double> previous_{};
    double seconds_ = 0;
    bool suspended_ = false;
};
}  // namespace rhythm::runtime
