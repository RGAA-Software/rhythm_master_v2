#pragma once

#include <span>
#include <vector>

namespace rhythm::parameters {
enum class Interpolation { kStep, kLinear, kSmooth, kHermite };
struct Keyframe {
    double seconds_ = 0;
    double value_ = 0;
    // The outgoing segment's interpolation. Smooth uses zero-slope smoothstep.
    Interpolation interpolation_ = Interpolation::kLinear;
    // Derivatives in value units per second. Hermite uses the left key's
    // outgoing slope and the right key's incoming slope; Smooth keeps its
    // original zero-slope behavior for existing projects.
    double in_slope_ = 0;
    double out_slope_ = 0;
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
struct CurveTransform {
    double time_offset_ = 0;
    double time_scale_ = 1;
    double time_pivot_ = 0;
    double value_offset_ = 0;
    double value_scale_ = 1;
    double value_pivot_ = 0;
};
// Atomic, value-returning bulk edit. Positive time scale preserves tangent
// direction. Reorders moved keys; collisions/invalid ranges reject the edit.
Curve TransformKeys(const Curve& curve, std::span<const std::size_t> selection,
                    const CurveTransform& transform);
}  // namespace rhythm::parameters
