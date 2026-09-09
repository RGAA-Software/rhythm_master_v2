#include "affine.h"

#include <algorithm>

#include "rhythm/geometry2d/affine.h"

namespace rhythm::runtime::detail {
void DrawAffine(const graph::Instruction& instruction, std::span<const NodeOutput> outputs,
                render::DrawList& list) {
    const auto& node = instruction.node_;
    const auto value = [&](std::size_t port, std::string_view key, double fallback, double minimum,
                           double maximum) {
        const auto slot = instruction.inputs_.at(port);
        return std::clamp(slot ? outputs[*slot].scalar_ : graph::Scalar(node, key, fallback),
                          minimum, maximum);
    };
    const auto scale = value(1, "scale", 1, 0, 8);
    const auto scale_x = scale * graph::Scalar(node, "scale_x", 1);
    const auto scale_y = scale * graph::Scalar(node, "scale_y", 1);
    const auto angle = value(2, "rotation", 0, -36000, 36000);
    const auto x = value(3, "translate_x", 0, -4, 4);
    const auto y = value(4, "translate_y", 0, -4, 4);
    const auto opacity = value(5, "opacity", 1, 0, 1);
    if (scale_x == 0 || scale_y == 0 || opacity == 0) return;
    const geometry2d::Pose pose{
            {x, y},
            {graph::Scalar(node, "pivot_x", 0.5), graph::Scalar(node, "pivot_y", 0.5)},
            {scale_x, scale_y},
            angle};
    const auto transform = geometry2d::Compose(pose, {list.width_, list.height_});
    const auto color = 0x00ffffffu | static_cast<std::uint32_t>(opacity * 255 + 0.5) << 24;
    const auto base = static_cast<std::uint32_t>(list.vertices_.size());
    const auto first = static_cast<std::uint32_t>(list.indices_.size());
    for (const auto& uv : {std::pair{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}}) {
        const auto point = geometry2d::Transform(
                transform, {uv.first * list.width_, uv.second * list.height_});
        list.vertices_.push_back({static_cast<float>(point.x_), static_cast<float>(point.y_),
                                  static_cast<float>(uv.first), static_cast<float>(uv.second),
                                  color});
    }
    list.indices_.insert(list.indices_.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    list.commands_.push_back({outputs[instruction.inputs_[0].value()].texture_,
                              first,
                              6,
                              {0, 0, list.width_, list.height_}});
}
}  // namespace rhythm::runtime::detail
