#include "rhythm/scene/animation.h"

#include <algorithm>
#include <cmath>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/spline.hpp>
#include <set>
#include <stdexcept>
#include <utility>

namespace rhythm::scene {
namespace {
void Require(bool condition) {
    if (!condition) throw std::invalid_argument("scene.animation");
}
bool Bounded(double value, double maximum = 1e6) {
    return std::isfinite(value) && std::abs(value) <= maximum;
}
glm::dvec4 Native(const std::array<double, 4>& value) {
    return {value[0], value[1], value[2], value[3]};
}
glm::dquat Native(Quaternion value) { return {value.w_, value.x_, value.y_, value.z_}; }
Quaternion Rotation(glm::dvec4 value) {
    const auto length = glm::length(value);
    Require(std::isfinite(length) && length > 1e-12);
    value /= length;
    return {value.x, value.y, value.z, value.w};
}
Quaternion MixRotation(Quaternion first, Quaternion second, double amount) {
    const auto value = glm::normalize(
            glm::slerp(glm::normalize(Native(first)), glm::normalize(Native(second)), amount));
    return {value.x, value.y, value.z, value.w};
}
glm::dvec4 SampleTrack(const AnimationTrack& track, double seconds) {
    const bool cubic = track.interpolation_ == AnimationInterpolation::kCubicSpline;
    const std::size_t stride = cubic ? 3 : 1;
    const std::size_t offset = cubic ? 1 : 0;
    const auto upper = std::upper_bound(track.times_.begin(), track.times_.end(), seconds);
    if (upper == track.times_.begin()) return Native(track.values_[offset]);
    const auto index = static_cast<std::size_t>(upper - track.times_.begin() - 1);
    const auto first = Native(track.values_[index * stride + offset]);
    if (upper == track.times_.end() || track.interpolation_ == AnimationInterpolation::kStep)
        return first;
    const auto next = Native(track.values_[(index + 1) * stride + offset]);
    const auto duration = track.times_[index + 1] - track.times_[index];
    const auto amount = (seconds - track.times_[index]) / duration;
    if (cubic) {
        // Godot glTF interval/tangent convention, evaluated with existing GLM.
        // Unlike Godot 4.5.1's quaternion fallback, glTF cubic rotation uses
        // component Hermite followed by normalization, without hemisphere flips.
        return glm::hermite(first, Native(track.values_[index * 3 + 2]) * duration, next,
                            Native(track.values_[(index + 1) * 3]) * duration, amount);
    }
    if (track.property_ == AnimationProperty::kRotation) {
        const auto value = MixRotation(Rotation(first), Rotation(next), amount);
        return {value.x_, value.y_, value.z_, value.w_};
    }
    return glm::mix(first, next, amount);
}
Vector3 MixVector(Vector3 first, Vector3 second, double amount) {
    const auto value = glm::mix(glm::dvec3(first.x_, first.y_, first.z_),
                                glm::dvec3(second.x_, second.y_, second.z_), amount);
    return {value.x, value.y, value.z};
}
}  // namespace
AnimationClip::AnimationClip(std::string name, std::vector<AnimationTrack> tracks)
    : name_(std::move(name)), tracks_(std::move(tracks)) {
    Require(name_.size() <= 1024 && tracks_.size() <= 8192);
    std::set<std::pair<std::uint64_t, AnimationProperty>> targets;
    std::size_t values = 0;
    for (const auto& track : tracks_) {
        Require(track.node_ && targets.emplace(track.node_, track.property_).second);
        Require(track.property_ == AnimationProperty::kTranslation ||
                track.property_ == AnimationProperty::kRotation ||
                track.property_ == AnimationProperty::kScale ||
                track.property_ == AnimationProperty::kWeights);
        Require(track.interpolation_ == AnimationInterpolation::kStep ||
                track.interpolation_ == AnimationInterpolation::kLinear ||
                track.interpolation_ == AnimationInterpolation::kCubicSpline);
        Require(track.components_ >= 1 && track.components_ <= 4);
        Require(track.property_ == AnimationProperty::kWeights ||
                track.components_ == (track.property_ == AnimationProperty::kRotation ? 4U : 3U));
        const std::size_t stride =
                track.interpolation_ == AnimationInterpolation::kCubicSpline ? 3 : 1;
        Require(!track.times_.empty() && track.times_.size() <= 262144 &&
                track.values_.size() == track.times_.size() * stride &&
                track.values_.size() <= 262144 - values);
        values += track.values_.size();
        double previous = -1;
        for (const auto time : track.times_) {
            Require(Bounded(time, 86400) && time > previous);
            previous = time;
        }
        Require(track.times_.front() >= 0);
        duration_ = std::max(duration_, track.times_.back());
        for (std::size_t index = 0; index < track.values_.size(); ++index) {
            const auto& value = track.values_[index];
            for (std::size_t component = 0; component < 4; ++component)
                Require(Bounded(value[component]) &&
                        (component < track.components_ || value[component] == 0));
            if (track.property_ == AnimationProperty::kRotation && (stride == 1 || index % 3 == 1))
                Require(std::abs(glm::length(Native(value)) - 1) < 0.01);
        }
    }
}
void Validate(const AnimationPose& pose) {
    Require(pose.size() <= 2048);
    for (const auto& [id, node] : pose) {
        Require(id != 0);
        Require(!node.matrix_ || ValidAffine(*node.matrix_));
        for (const auto value : {node.translation_.x_, node.translation_.y_, node.translation_.z_,
                                 node.scale_.x_, node.scale_.y_, node.scale_.z_})
            Require(Bounded(value));
        Require(std::abs(node.scale_.x_) >= 1e-6 && std::abs(node.scale_.y_) >= 1e-6 &&
                std::abs(node.scale_.z_) >= 1e-6);
        Require(std::abs(glm::length(Native(node.rotation_)) - 1) < 0.01);
        for (const auto value : node.weights_) Require(Bounded(value, 100));
    }
}
AnimationPose Sample(const AnimationClip& clip, const AnimationPose& rest, double seconds,
                     bool loop) {
    Require(Bounded(seconds, 1e9));
    Validate(rest);
    if (loop && clip.Duration() > 0) {
        seconds = std::fmod(seconds, clip.Duration());
        if (seconds < 0) seconds += clip.Duration();
    }
    auto result = rest;
    for (const auto& track : clip.Tracks()) {
        Require(result.contains(track.node_));
        auto& node = result.at(track.node_);
        Require(!node.matrix_ || track.property_ == AnimationProperty::kWeights);
        const auto value = SampleTrack(track, seconds);
        switch (track.property_) {
            case AnimationProperty::kTranslation:
                node.translation_ = {value.x, value.y, value.z};
                break;
            case AnimationProperty::kRotation:
                node.rotation_ = Rotation(value);
                break;
            case AnimationProperty::kScale:
                node.scale_ = {value.x, value.y, value.z};
                break;
            case AnimationProperty::kWeights:
                node.weights_ = {value.x, value.y, value.z, value.w};
                break;
        }
    }
    Validate(result);
    return result;
}
AnimationPose Blend(const AnimationPose& first, const AnimationPose& second, double amount) {
    Require(std::isfinite(amount) && amount >= 0 && amount <= 1 && first.size() == second.size());
    Validate(first);
    Validate(second);
    auto result = first;
    for (auto& [id, node] : result) {
        Require(second.contains(id));
        const auto& other = second.at(id);
        Require(node.matrix_ == other.matrix_);
        node.translation_ = MixVector(node.translation_, other.translation_, amount);
        node.rotation_ = MixRotation(node.rotation_, other.rotation_, amount);
        node.scale_ = MixVector(node.scale_, other.scale_, amount);
        for (std::size_t i = 0; i < 4; ++i)
            node.weights_[i] = std::lerp(node.weights_[i], other.weights_[i], amount);
    }
    Validate(result);
    return result;
}
}  // namespace rhythm::scene
