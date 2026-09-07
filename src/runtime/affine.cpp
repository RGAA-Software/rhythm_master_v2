#include "affine.h"

#include <algorithm>
#include <cmath>
#include <numbers>

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
    const auto angle = value(2, "rotation", 0, -36000, 36000) * std::numbers::pi / 180;
    const auto x = value(3, "translate_x", 0, -4, 4) * list.width_;
    const auto y = value(4, "translate_y", 0, -4, 4) * list.height_;
    const auto opacity = value(5, "opacity", 1, 0, 1);
    if (scale_x == 0 || scale_y == 0 || opacity == 0) return;
    const auto pivot_x = graph::Scalar(node, "pivot_x", 0.5) * list.width_;
    const auto pivot_y = graph::Scalar(node, "pivot_y", 0.5) * list.height_;
    const auto cosine = std::cos(angle), sine = std::sin(angle);
    const auto color = 0x00ffffffu | static_cast<std::uint32_t>(opacity * 255 + 0.5) << 24;
    const auto base = static_cast<std::uint32_t>(list.vertices_.size());
    const auto first = static_cast<std::uint32_t>(list.indices_.size());
    for (const auto& uv : {std::pair{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}}) {
        const auto local_x = (uv.first * list.width_ - pivot_x) * scale_x;
        const auto local_y = (uv.second * list.height_ - pivot_y) * scale_y;
        list.vertices_.push_back(
                {static_cast<float>(pivot_x + x + cosine * local_x - sine * local_y),
                 static_cast<float>(pivot_y + y + sine * local_x + cosine * local_y),
                 static_cast<float>(uv.first), static_cast<float>(uv.second), color});
    }
    list.indices_.insert(list.indices_.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    list.commands_.push_back({outputs[instruction.inputs_[0].value()].texture_,
                              first,
                              6,
                              {0, 0, list.width_, list.height_}});
}
}  // namespace rhythm::runtime::detail
