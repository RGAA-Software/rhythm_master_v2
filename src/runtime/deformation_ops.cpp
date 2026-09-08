#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "scene_ops.h"

namespace rhythm::runtime::detail {
void EvaluateDeformation(const graph::Instruction& instruction, std::span<const NodeOutput> outputs,
                         NodeOutput& output) {
    const auto& node = instruction.node_;
    const auto input = [&](std::size_t port) -> const NodeOutput& {
        if (port >= instruction.inputs_.size() || !instruction.inputs_[port] ||
            *instruction.inputs_[port] >= outputs.size())
            throw std::invalid_argument("runtime.deformation_input");
        return outputs[*instruction.inputs_[port]];
    };
    const auto scalar = [&](std::size_t port, std::string_view key, double fallback, double bound) {
        const auto value = instruction.inputs_.at(port) ? input(port).scalar_
                                                        : graph::Scalar(node, key, fallback);
        return std::isfinite(value) ? std::clamp(value, -bound, bound) : fallback;
    };
    if (!input(0).geometry_ || !input(0).geometry_->model_)
        throw std::invalid_argument("runtime.deformation_input");
    auto geometry = *input(0).geometry_;
    if (geometry.deformations_.size() >= 4) throw std::length_error("render.deformation_limit");
    if (!geometry.upload_id_) {
        geometry.upload_id_ = geometry.id_;
        geometry.upload_revision_ = geometry.revision_;
    }
    geometry.id_ = node.id_;
    geometry.revision_ = output.version_;
    geometry.deformations_.push_back(
            {scalar(1, "deform_twist", 45, 720),
             scalar(2, "deform_taper", 0, 4),
             std::uint32_t(graph::Scalar(node, "deform_axis", 1)),
             {graph::Scalar(node, "deform_pivot_x", 0), graph::Scalar(node, "deform_pivot_y", 0),
              graph::Scalar(node, "deform_pivot_z", 0)}});
    output.geometry_ = std::make_shared<const scene::Geometry>(std::move(geometry));
}
}  // namespace rhythm::runtime::detail
