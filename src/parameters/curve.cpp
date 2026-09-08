#include "rhythm/parameters/curve.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace rhythm::parameters {
Curve::Curve(std::vector<Keyframe> keys) { SetKeys(std::move(keys)); }
void Curve::SetKeys(std::vector<Keyframe> keys) {
    if (keys.empty() || keys.size() > kMaximumKeys) throw std::length_error("curve.key_count");
    double previous = -1;
    for (const auto& key : keys) {
        if (!std::isfinite(key.seconds_) || key.seconds_ < 0 || key.seconds_ > 1.0e9 ||
            key.seconds_ <= previous || !std::isfinite(key.value_) ||
            std::abs(key.value_) > 1.0e12 || !std::isfinite(key.in_slope_) ||
            std::abs(key.in_slope_) > 1.0e12 || !std::isfinite(key.out_slope_) ||
            std::abs(key.out_slope_) > 1.0e12 ||
            (key.interpolation_ != Interpolation::kStep &&
             key.interpolation_ != Interpolation::kLinear &&
             key.interpolation_ != Interpolation::kSmooth &&
             key.interpolation_ != Interpolation::kHermite))
            throw std::invalid_argument("curve.key_value");
        previous = key.seconds_;
    }
    keys_ = std::move(keys);
}
double Curve::Evaluate(double seconds) const {
    if (keys_.empty()) throw std::logic_error("curve.empty_moved_value");
    if (!std::isfinite(seconds)) throw std::invalid_argument("curve.time");
    if (seconds <= keys_.front().seconds_) return keys_.front().value_;
    if (seconds >= keys_.back().seconds_) return keys_.back().value_;
    const auto right =
            std::upper_bound(keys_.begin(), keys_.end(), seconds,
                             [](double time, const Keyframe& key) { return time < key.seconds_; });
    const auto& left = *(right - 1);
    if (left.interpolation_ == Interpolation::kStep) return left.value_;
    auto factor = (seconds - left.seconds_) / (right->seconds_ - left.seconds_);
    if (left.interpolation_ == Interpolation::kHermite) {
        // TiXL SplineInterpolator.Interpolate: derivative times segment length,
        // with independent incoming/outgoing tangents. See provenance/curves.json.
        const auto duration = right->seconds_ - left.seconds_;
        const auto t2 = factor * factor, t3 = t2 * factor;
        return (2 * t3 - 3 * t2 + 1) * left.value_ +
               (t3 - 2 * t2 + factor) * left.out_slope_ * duration +
               (-2 * t3 + 3 * t2) * right->value_ + (t3 - t2) * right->in_slope_ * duration;
    }
    if (left.interpolation_ == Interpolation::kSmooth) factor = factor * factor * (3 - 2 * factor);
    return std::lerp(left.value_, right->value_, factor);
}
Curve TransformKeys(const Curve& curve, std::span<const std::size_t> selection,
                    const CurveTransform& transform) {
    for (const auto value :
         {transform.time_offset_, transform.time_scale_, transform.time_pivot_,
          transform.value_offset_, transform.value_scale_, transform.value_pivot_})
        if (!std::isfinite(value)) throw std::invalid_argument("curve.transform");
    if (transform.time_scale_ <= 0 || selection.size() > curve.Keys().size())
        throw std::invalid_argument("curve.transform");
    auto keys = std::vector<Keyframe>(curve.Keys().begin(), curve.Keys().end());
    std::vector<bool> selected(keys.size());
    for (const auto index : selection) {
        if (index >= keys.size() || selected[index]) throw std::invalid_argument("curve.selection");
        selected[index] = true;
        auto& key = keys[index];
        key.seconds_ = transform.time_pivot_ +
                       (key.seconds_ - transform.time_pivot_) * transform.time_scale_ +
                       transform.time_offset_;
        key.value_ = transform.value_pivot_ +
                     (key.value_ - transform.value_pivot_) * transform.value_scale_ +
                     transform.value_offset_;
        key.in_slope_ *= transform.value_scale_ / transform.time_scale_;
        key.out_slope_ *= transform.value_scale_ / transform.time_scale_;
    }
    std::sort(keys.begin(), keys.end(),
              [](const auto& a, const auto& b) { return a.seconds_ < b.seconds_; });
    return Curve(std::move(keys));
}
}  // namespace rhythm::parameters
