#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "scene_ops.h"

namespace rhythm::runtime::detail {
void EvaluateMorph(const graph::Instruction& instruction, std::span<const NodeOutput> outputs,
                   NodeOutput& output) {
    const auto input = [&](std::size_t port) -> const NodeOutput& {
        if (port >= instruction.inputs_.size() || !instruction.inputs_[port] ||
            *instruction.inputs_[port] >= outputs.size())
            throw std::invalid_argument("runtime.morph_input");
        return outputs[*instruction.inputs_[port]];
    };
    if (!input(0).geometry_ || !input(0).geometry_->model_)
        throw std::invalid_argument("runtime.morph_input");
    auto geometry = *input(0).geometry_;
    const auto& model = *geometry.model_;
    auto pose = geometry.pose_ ? *geometry.pose_ : model.rest_pose_;
    constexpr std::array<std::string_view, 4> kKeys{"morph_weight_1", "morph_weight_2",
                                                    "morph_weight_3", "morph_weight_4"};
    std::array<double, 4> weights{};
    for (std::size_t i = 0; i < weights.size(); ++i) {
        const auto value = instruction.inputs_.at(i + 1)
                                   ? input(i + 1).scalar_
                                   : graph::Scalar(instruction.node_, kKeys[i], 0);
        if (!std::isfinite(value)) throw std::invalid_argument("runtime.morph_weight");
        weights[i] = std::clamp(value, -4.0, 4.0);
    }
    bool found = false;
    for (const auto& node : model.nodes_) {
        std::size_t count = 0;
        for (const auto index : node.meshes_)
            count = std::max(count, model.meshes_.at(index).morphs_.size());
        if (!count) continue;
        found = true;
        auto& target = pose.at(node.id_).weights_;
        target = {};
        std::copy_n(weights.begin(), count, target.begin());
    }
    if (!found) throw std::invalid_argument("runtime.morph_input");
    if (!geometry.upload_id_) {
        geometry.upload_id_ = geometry.id_;
        geometry.upload_revision_ = geometry.revision_;
    }
    geometry.id_ = instruction.node_.id_;
    geometry.revision_ = output.version_;
    geometry.pose_ = std::make_shared<const scene::AnimationPose>(std::move(pose));
    output.geometry_ = std::make_shared<const scene::Geometry>(std::move(geometry));
}
void EvaluateAnimation(const graph::Instruction& instruction, std::span<const NodeOutput> outputs,
                       NodeOutput& output, double seconds) {
    const auto& node = instruction.node_;
    const auto input = [&](std::size_t port) -> const NodeOutput& {
        if (port >= instruction.inputs_.size() || !instruction.inputs_[port] ||
            *instruction.inputs_[port] >= outputs.size())
            throw std::invalid_argument("runtime.animation_input");
        return outputs[*instruction.inputs_[port]];
    };
    if (!input(0).geometry_ || !input(0).geometry_->model_)
        throw std::invalid_argument("runtime.animation_input");
    auto geometry = *input(0).geometry_;
    const auto& model = *geometry.model_;
    const auto clip_index = [&](std::string_view key) {
        const auto value = graph::Scalar(node, key, 0);
        if (!std::isfinite(value) || value < 0 || std::floor(value) != value ||
            value >= static_cast<double>(model.animations_.size()))
            throw std::invalid_argument("runtime.animation_clip");
        return static_cast<std::size_t>(value);
    };
    const auto time = (instruction.inputs_[1] ? input(1).scalar_ : seconds) *
                              graph::Scalar(node, "animation_speed", 1) +
                      graph::Scalar(node, "animation_offset", 0);
    const auto blend_value =
            instruction.inputs_[2] ? input(2).scalar_ : graph::Scalar(node, "animation_blend", 0);
    if (!std::isfinite(blend_value)) throw std::invalid_argument("runtime.animation_blend");
    const auto blend = std::clamp(blend_value, 0.0, 1.0);
    const auto loop = graph::Scalar(node, "animation_loop", 1) != 0;
    auto pose = scene::Sample(model.animations_.at(clip_index("animation_clip")), model.rest_pose_,
                              time, loop);
    if (blend > 0) {
        const auto second = scene::Sample(model.animations_.at(clip_index("animation_second")),
                                          model.rest_pose_, time, loop);
        pose = scene::Blend(pose, second, blend);
    }
    if (!geometry.upload_id_) {
        geometry.upload_id_ = geometry.id_;
        geometry.upload_revision_ = geometry.revision_;
    }
    geometry.id_ = node.id_;
    geometry.revision_ = output.version_;
    geometry.pose_ = std::make_shared<const scene::AnimationPose>(std::move(pose));
    output.geometry_ = std::make_shared<const scene::Geometry>(std::move(geometry));
}
}  // namespace rhythm::runtime::detail
