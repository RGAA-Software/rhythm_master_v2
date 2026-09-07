#pragma once

#include <span>
#include <vector>

namespace rhythm::parameters {
enum class Interpolation { kStep, kLinear, kSmooth };
struct Keyframe {
    double seconds_ = 0;
    double value_ = 0;
    // The outgoing segment's interpolation. Smooth uses zero-slope smoothstep.
    Interpolation interpolation_ = Interpolation::kLinear;
    bool operator==(const Keyframe&) const = default;
};
// Immutable reads are allocation-free. SetKeys validates before committing;
// callers own mutation/thread affinity. Local clocks supply loop/ping-pong time.
class Curve final {
   public:
    Curve() = default;
    explicit Curve(std::vector<Keyframe> keys);
    void SetKeys(std::vector<Keyframe> keys);
    [[nodiscard]] std::span<const Keyframe> Keys() const { return keys_; }
    [[nodiscard]] double Evaluate(double seconds) const;
    bool operator==(const Curve&) const = default;
    static constexpr std::size_t kMaximumKeys = 1024;

   private:
    std::vector<Keyframe> keys_{{0, 0}, {1, 1}};
};
}  // namespace rhythm::parameters
