#include "rhythm/parameters/beat_grid.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace rhythm::parameters {
namespace {
void CheckSeconds(double seconds) {
    if (!std::isfinite(seconds) || seconds < 0 || seconds > kMaximumBeatSeconds)
        throw std::invalid_argument("beat.time");
}
bool ValidUnit(std::uint32_t unit) { return unit && unit <= 32 && (unit & (unit - 1)) == 0; }
}  // namespace
bool ValidBeatSettings(const BeatSettings& settings) {
    return std::isfinite(settings.bpm_) && settings.bpm_ >= 20 && settings.bpm_ <= 600 &&
           settings.beats_per_bar_ >= 1 && settings.beats_per_bar_ <= 32 &&
           ValidUnit(settings.beat_unit_) && std::isfinite(settings.origin_seconds_) &&
           std::abs(settings.origin_seconds_) <= kMaximumBeatSeconds;
}
BeatGrid::BeatGrid(BeatSettings settings) : settings_(settings) {
    if (!ValidBeatSettings(settings_)) throw std::invalid_argument("beat.settings");
}
double BeatGrid::BeatDuration() const { return 60 / settings_.bpm_ * 4 / settings_.beat_unit_; }
double BeatGrid::BarDuration() const { return BeatDuration() * settings_.beats_per_bar_; }
double BeatGrid::Boundary(std::int64_t index, double duration) const {
    return std::fma(static_cast<double>(index), duration, settings_.origin_seconds_);
}
std::int64_t BeatGrid::IndexAt(double seconds, double duration) const {
    auto index =
            static_cast<std::int64_t>(std::floor((seconds - settings_.origin_seconds_) / duration));
    // Division may round a one-ULP-before request onto the following index.
    // Compare the actual representable boundary; an arbitrary epsilon would
    // incorrectly delay or advance requests next to a genuine musical boundary.
    if (Boundary(index, duration) > seconds)
        --index;
    else if (Boundary(index + 1, duration) <= seconds)
        ++index;
    return index;
}
BeatPosition BeatGrid::Position(double seconds) const {
    CheckSeconds(seconds);
    const auto duration = BeatDuration();
    const auto beat = IndexAt(seconds, duration);
    const auto count = static_cast<std::int64_t>(settings_.beats_per_bar_);
    auto bar = beat / count;
    auto in_bar = beat % count;
    if (in_bar < 0) {
        --bar;
        in_bar += count;
    }
    const auto phase = std::clamp((seconds - Boundary(beat, duration)) / duration, 0.0,
                                  std::nextafter(1.0, 0.0));
    return {beat, bar, static_cast<std::uint32_t>(in_bar), phase};
}
std::optional<double> BeatGrid::NextAfter(double seconds, Quantization quantization) const {
    CheckSeconds(seconds);
    if (quantization == Quantization::kImmediate) return seconds;
    if (quantization != Quantization::kBeat && quantization != Quantization::kBar)
        throw std::invalid_argument("beat.quantization");
    const auto duration = quantization == Quantization::kBeat ? BeatDuration() : BarDuration();
    const auto next = Boundary(IndexAt(seconds, duration) + 1, duration);
    return next > seconds && next <= kMaximumBeatSeconds ? std::optional(next) : std::nullopt;
}
std::optional<double> TapTempo::Tap(double seconds, std::uint32_t beat_unit) {
    CheckSeconds(seconds);
    if (!ValidUnit(beat_unit)) throw std::invalid_argument("beat.unit");
    if (beat_unit != beat_unit_) {
        Reset();
        beat_unit_ = beat_unit;
    }
    if (count_) {
        const auto interval = seconds - taps_[count_ - 1];
        if (interval <= 0 || interval > 60.0 / 20 * 4 / beat_unit + 1e-6)
            Reset();
        else if (interval < 60.0 / 600 * 4 / beat_unit - 1e-6)
            return {};
    }
    if (count_ == taps_.size()) {
        std::move(taps_.begin() + 1, taps_.end(), taps_.begin());
        --count_;
    }
    taps_[count_++] = seconds;
    if (count_ < 4) return {};
    // Adapted interval averaging from TiXL BeatTiming.ProcessBeatTaps (MIT).
    // No global Playback/BeatSynchronizer, phase smoothing or runtime clock is
    // imported. Source, exact revision and retained notice: provenance/beat_grid.json.
    const auto mean = (taps_[count_ - 1] - taps_[0]) / static_cast<double>(count_ - 1);
    return std::clamp(60 / mean * 4 / beat_unit, 20.0, 600.0);
}
}  // namespace rhythm::parameters
