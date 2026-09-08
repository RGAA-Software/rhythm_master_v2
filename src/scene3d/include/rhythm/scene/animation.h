#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <vector>

#include "rhythm/scene/math.h"

namespace rhythm::scene {
enum class AnimationProperty { kTranslation, kRotation, kScale, kWeights };
enum class AnimationInterpolation { kStep, kLinear, kCubicSpline };
struct AnimationTrack {
    std::uint64_t node_ = 0;
    AnimationProperty property_ = AnimationProperty::kTranslation;
    AnimationInterpolation interpolation_ = AnimationInterpolation::kLinear;
    std::uint32_t components_ = 3;
    std::vector<double> times_{};
    // Cubic keys are [incoming tangent, value, outgoing tangent], in glTF order.
    // Rotation is xyzw; morph weights use components_ (1..4).
    std::vector<std::array<double, 4>> values_{};
};
// Validated immutable tracks. Sampling performs binary search, not full key scans.
// Limits: 8192 tracks, 262144 total stored values, 24 hours, four morph weights.
class AnimationClip {
   public:
    AnimationClip() = default;
    AnimationClip(std::string name, std::vector<AnimationTrack> tracks);
    const std::string& Name() const { return name_; }
    std::span<const AnimationTrack> Tracks() const { return tracks_; }
    double Duration() const { return duration_; }

   private:
    std::string name_{};
    std::vector<AnimationTrack> tracks_{};
    double duration_ = 0;
};
struct NodePose {
    Vector3 translation_{};
    Quaternion rotation_{};
    Vector3 scale_{1, 1, 1};
    std::array<double, 4> weights_{};
};
using AnimationPose = std::map<std::uint64_t, NodePose>;
void Validate(const AnimationPose& pose);
// Uses caller-owned unified transport seconds. No clock, accumulation or history.
// Missing channels retain rest values. Loop wraps at duration; clamp holds endpoints.
AnimationPose Sample(const AnimationClip& clip, const AnimationPose& rest, double seconds,
                     bool loop);
// Both poses must address the same node set. Rotations use shortest-arc slerp.
AnimationPose Blend(const AnimationPose& first, const AnimationPose& second, double amount);
}  // namespace rhythm::scene
