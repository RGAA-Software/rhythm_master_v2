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
            std::abs(key.value_) > 1.0e12 ||
            (key.interpolation_ != Interpolation::kStep &&
             key.interpolation_ != Interpolation::kLinear &&
             key.interpolation_ != Interpolation::kSmooth))
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
    if (left.interpolation_ == Interpolation::kSmooth) factor = factor * factor * (3 - 2 * factor);
    return std::lerp(left.value_, right->value_, factor);
}
}  // namespace rhythm::parameters
