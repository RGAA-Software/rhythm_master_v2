#include "rhythm/parameters/clip_interval.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "rhythm/parameters/curve.h"

namespace rhythm::parameters {
double EnvelopeGain(double local_seconds, double duration, double fade_in, double fade_out,
                    bool smooth) {
    if (!std::isfinite(local_seconds) || !std::isfinite(duration) || duration <= 0 ||
        !std::isfinite(fade_in) || !std::isfinite(fade_out) || fade_in < 0 || fade_out < 0)
        throw std::invalid_argument("clip.envelope");
    const auto sum = fade_in + fade_out;
    if (!std::isfinite(sum)) throw std::invalid_argument("clip.envelope");
    if (local_seconds < 0 || local_seconds >= duration) return 0;
    if (sum > duration) {
        fade_in *= duration / sum;
        fade_out *= duration / sum;
    }
    auto value = fade_in > 0 ? std::min(1.0, local_seconds / fade_in) : 1.0;
    if (fade_out > 0) value = std::min(value, (duration - local_seconds) / fade_out);
    if (!smooth) return value;
    static const Curve kRamp({{0, 0, Interpolation::kSmooth}, {1, 1, Interpolation::kLinear}});
    return kRamp.Evaluate(value);
}
ClipInterval::ClipInterval(ClipTiming timing) : timing_(timing) {
    for (const auto value : {timing.start_, timing.duration_, timing.source_in_, timing.source_out_,
                             timing.fade_in_, timing.fade_out_})
        if (!std::isfinite(value) || value < 0 || value > kMaximumSeconds)
            throw std::invalid_argument("clip.time");
    if (timing.duration_ <= 0 || timing.start_ + timing.duration_ > kMaximumSeconds ||
        timing.source_in_ >= timing.source_out_ || !std::isfinite(timing.rate_) ||
        timing.rate_ < 0.125 || timing.rate_ > 8 ||
        (timing.end_ != ClipEnd::kBlank && timing.end_ != ClipEnd::kHold &&
         timing.end_ != ClipEnd::kLoop))
        throw std::invalid_argument("clip.interval");
}
double ClipInterval::ActiveDuration() const {
    return timing_.end_ == ClipEnd::kBlank
                   ? std::min(timing_.duration_,
                              (timing_.source_out_ - timing_.source_in_) / timing_.rate_)
                   : timing_.duration_;
}
ClipSample ClipInterval::Sample(double seconds) const {
    if (!std::isfinite(seconds) || seconds < 0) throw std::invalid_argument("clip.time");
    const auto local = seconds - timing_.start_;
    const auto duration = ActiveDuration();
    if (local < 0 || local >= duration) return {};
    const auto span = timing_.source_out_ - timing_.source_in_;
    auto source = local * timing_.rate_;
    if (timing_.end_ == ClipEnd::kLoop) source = std::fmod(source, span);
    const auto upper = std::nextafter(timing_.source_out_, timing_.source_in_);
    const auto target = std::min(upper, timing_.source_in_ + source);
    return {true, target,
            EnvelopeGain(local, duration, timing_.fade_in_, timing_.fade_out_, timing_.smooth_)};
}
}  // namespace rhythm::parameters
