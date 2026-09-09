#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace rhythm::parameters {
inline constexpr double kMaximumBeatSeconds = 1e9;
// BPM always counts quarter notes. A beat uses the notated denominator: at
// 120 BPM, 6/8 has 0.25-second beats and 1.5-second bars. Origin is bar 0/beat 0.
struct BeatSettings {
    double bpm_ = 120;
    std::uint32_t beats_per_bar_ = 4;
    std::uint32_t beat_unit_ = 4;
    double origin_seconds_ = 0;
    bool operator==(const BeatSettings&) const = default;
};
bool ValidBeatSettings(const BeatSettings& settings);
enum class Quantization { kImmediate, kBeat, kBar };
struct BeatPosition {
    std::int64_t beat_ = 0;
    std::int64_t bar_ = 0;
    std::uint32_t beat_in_bar_ = 0;
    double phase_ = 0;
};
// Immutable grid, not a clock. The host supplies presentation/media time.
// Queries accept [0, 1e9] seconds; the grid extends backwards before its origin.
class BeatGrid final {
   public:
    explicit BeatGrid(BeatSettings settings = {});
    const BeatSettings& Settings() const { return settings_; }
    double BeatDuration() const;
    double BarDuration() const;
    BeatPosition Position(double seconds) const;
    // Strictly after the request, including exactly-on-boundary requests.
    // Immediate returns the supplied time. Empty means no boundary in budget.
    std::optional<double> NextAfter(double seconds, Quantization quantization) const;

   private:
    std::int64_t IndexAt(double seconds, double duration) const;
    double Boundary(std::int64_t index, double duration) const;
    BeatSettings settings_{};
};
// Host-thread manual tap estimator: eight accepted timestamps, four taps before
// a result. Uses caller-supplied monotonic timestamps, never drives playback.
class TapTempo final {
   public:
    std::optional<double> Tap(double seconds, std::uint32_t beat_unit = 4);
    void Reset() { count_ = 0; }
    std::size_t Count() const { return count_; }

   private:
    std::array<double, 8> taps_{};
    std::size_t count_ = 0;
    std::uint32_t beat_unit_ = 4;
};
}  // namespace rhythm::parameters
