#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "rhythm/scene/animation.h"
#include "rhythm/scene/model.h"

namespace {
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void Near(double value, double expected) {
    Require(std::abs(value - expected) < 1e-10, "animation sample differs from reference");
}
template <typename Function>
void Reject(Function function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return;
    }
    throw std::runtime_error("invalid animation accepted");
}
void Run() {
    using namespace rhythm::scene;
    AnimationPose rest{{1, {}}, {2, {}}};
    rest.at(1).translation_.y_ = 7;
    rest.at(2).weights_[0] = 0.3;
    AnimationTrack translation{
            1,      AnimationProperty::kTranslation, AnimationInterpolation::kLinear, 3,
            {1, 3}, {{0, 2, 0, 0}, {4, 6, 0, 0}}};
    AnimationTrack rotation{
            1,      AnimationProperty::kRotation, AnimationInterpolation::kLinear, 4,
            {0, 3}, {{0, 0, 0, 1}, {0, 0, 1, 0}}};
    AnimationClip clip("turn", {translation, rotation});
    Near(clip.Duration(), 3);
    const auto midpoint = Sample(clip, rest, 1.5, false);
    Near(midpoint.at(1).translation_.x_, 1);
    Near(midpoint.at(1).rotation_.z_, std::sqrt(0.5));
    Near(midpoint.at(2).weights_[0], 0.3);
    Near(Sample(clip, rest, -5, false).at(1).translation_.y_, 2);
    Near(Sample(clip, rest, 99, false).at(1).translation_.x_, 4);
    Near(Sample(clip, rest, 3, true).at(1).translation_.x_, 0);
    Near(Sample(clip, rest, -1, true).at(1).translation_.x_, 2);
    // Repeated seeks do not accumulate previous samples or mutate immutable tracks/rest.
    for (int i = 0; i < 100; ++i) {
        (void)Sample(clip, rest, static_cast<double>(i), false);
        Near(Sample(clip, rest, 1.5, false).at(1).translation_.x_, 1);
    }
    Near(rest.at(1).translation_.y_, 7);
    translation.interpolation_ = AnimationInterpolation::kStep;
    Near(Sample(AnimationClip("step", {translation}), rest, 2.999, false).at(1).translation_.x_, 0);
    Near(Sample(AnimationClip("step", {translation}), rest, 3, false).at(1).translation_.x_, 4);
    translation.interpolation_ = AnimationInterpolation::kCubicSpline;
    translation.times_ = {0, 2};
    translation.values_ = {{0, 0, 0, 0}, {0, 0, 0, 0}, {2, 0, 0, 0},
                           {0, 0, 0, 0}, {1, 0, 0, 0}, {0, 0, 0, 0}};
    Near(Sample(AnimationClip("cubic", {translation}), rest, 0.5, false).at(1).translation_.x_,
         0.71875);
    rotation.interpolation_ = AnimationInterpolation::kCubicSpline;
    rotation.times_ = {0, 2};
    rotation.values_ = {{0, 0, 0, 0}, {0, 0, 0, 1}, {0, 0, 1, 0},
                        {0, 0, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 0}};
    const auto cubic_rotation =
            Sample(AnimationClip("cubic", {rotation}), rest, 1, false).at(1).rotation_;
    Near(cubic_rotation.z_, 0.75 / std::hypot(0.75, 0.5));
    Near(cubic_rotation.w_, 0.5 / std::hypot(0.75, 0.5));
    rotation.interpolation_ = AnimationInterpolation::kLinear;
    rotation.values_ = {{0, 0, 0, 1}, {0, 0, 0, -1}};
    Near(std::abs(Sample(AnimationClip("shortest", {rotation}), rest, 1, false).at(1).rotation_.w_),
         1);
    AnimationTrack weights{2,      AnimationProperty::kWeights, AnimationInterpolation::kLinear, 2,
                           {0, 2}, {{0, 1, 0, 0}, {1, 0, 0, 0}}};
    const auto morphed = Sample(AnimationClip("morph", {weights}), rest, 0.5, false);
    Near(morphed.at(2).weights_[0], 0.25);
    Near(morphed.at(2).weights_[1], 0.75);
    const auto blended = Blend(rest, midpoint, 0.5);
    auto model = Cube();
    model.nodes_[0].meshes_.clear();
    model.nodes_.push_back({2, 1, {}, {0}, true, "child"});
    model.rest_pose_ = rest;
    model.animations_.push_back(clip);
    Validate(model);
    const auto worlds = WorldTransforms(model, midpoint);
    Near(worlds.at(2).transform_.values_[12], 1);
    Near(worlds.at(2).transform_.values_[13], 3);
    Near(blended.at(1).translation_.x_, 0.5);
    Near(blended.at(1).rotation_.z_, std::sin(std::acos(-1.0) / 8));
    Near(Blend(rest, midpoint, 0).at(1).translation_.y_, 7);
    Near(Blend(rest, midpoint, 1).at(1).translation_.x_, 1);
    Near(Sample(AnimationClip{}, rest, 100, true).at(1).translation_.y_, 7);
    weights.times_ = {0};
    weights.values_ = {{0.8, 0.2, 0, 0}};
    Near(Sample(AnimationClip("single", {weights}), rest, -1, true).at(2).weights_[0], 0.8);
    Reject([&] { AnimationClip("duplicate", {rotation, rotation}); });
    Reject([&] { Sample(clip, {}, 0, false); });
    Reject([&] { Sample(clip, rest, std::numeric_limits<double>::infinity(), true); });
    Reject([&] { Blend(rest, midpoint, -0.1); });
    Reject([&] { Blend(rest, {{3, {}}, {4, {}}}, 0.5); });
    auto invalid = rest;
    invalid.at(1).scale_.x_ = -1;
    Reject([&] { Blend(rest, invalid, 0.5); });
    rotation.times_ = {1, 1};
    Reject([&] { AnimationClip("duplicate time", {rotation}); });
    rotation.times_ = {-1, 1};
    Reject([&] { AnimationClip("negative time", {rotation}); });
    rotation.times_ = {0, 1};
    rotation.values_[0][3] = 0;
    Reject([&] { AnimationClip("zero quaternion", {rotation}); });
    weights.values_[0][0] = std::numeric_limits<double>::quiet_NaN();
    Reject([&] { AnimationClip("nan", {weights}); });
    weights.values_[0][0] = 0;
    weights.components_ = 5;
    Reject([&] { AnimationClip("weight budget", {weights}); });
}
}  // namespace
int main() {
    try {
        Run();
        std::cout << "animation interpolation, seek, loop, blend and admission passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
