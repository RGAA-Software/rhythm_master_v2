#pragma once

namespace rhythm::parameters {
enum class ClipEnd { kBlank, kHold, kLoop };
struct ClipTiming {
    double start_ = 0;
    double duration_ = 1;
    double source_in_ = 0;
    double source_out_ = 1;
    double rate_ = 1;
    ClipEnd end_ = ClipEnd::kBlank;
    double fade_in_ = 0;
    double fade_out_ = 0;
    bool smooth_ = true;
    bool operator==(const ClipTiming&) const = default;
};
struct ClipSample {
    bool active_ = false;
    double source_seconds_ = 0;
    double gain_ = 0;
    bool operator==(const ClipSample&) const = default;
};
// A time mapping, not a clock. Positive rates preserve source order; loop wraps
// only the selected source interval, and the placed interval is half-open.
class ClipInterval final {
   public:
    ClipInterval() = default;
    explicit ClipInterval(ClipTiming timing);
    const ClipTiming& Timing() const { return timing_; }
    ClipSample Sample(double seconds) const;
    double ActiveDuration() const;
    bool operator==(const ClipInterval&) const = default;
    static constexpr double kMaximumSeconds = 86400.0 * 7;

   private:
    ClipTiming timing_{};
};
// Shared envelope contract: overlapping fades scale proportionally to fit.
// Reuses the curve module's smoothstep shape; local time outside [0,duration)
// returns zero, with no allocation or hidden timeline state.
double EnvelopeGain(double local_seconds, double duration, double fade_in, double fade_out,
                    bool smooth);
}  // namespace rhythm::parameters
