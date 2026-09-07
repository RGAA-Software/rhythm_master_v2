#include "shapes.h"

#include <cmath>
#include <numbers>

namespace rhythm::runtime::detail {
void DrawShape(const graph::Node& node, render::TextureHandle white, render::DrawList& list) {
    const auto type = graph::Scalar(node, "shape_type", 1);
    const auto half_width = graph::Scalar(node, "shape_width", 0.8) * list.width_ * 0.5;
    const auto half_height = graph::Scalar(node, "shape_height", 0.8) * list.height_ * 0.5;
    const auto center_x = graph::Scalar(node, "center_x", 0.5) * list.width_;
    const auto center_y = graph::Scalar(node, "center_y", 0.5) * list.height_;
    const auto rgba = graph::ColorValue(node, "color_a", {1, 1, 1, 1});
    if (half_width == 0 || half_height == 0 || rgba.a_ == 0) return;
    const auto byte = [](double value) { return static_cast<std::uint32_t>(value * 255 + 0.5); };
    const auto color =
            byte(rgba.r_) | byte(rgba.g_) << 8 | byte(rgba.b_) << 16 | byte(rgba.a_) << 24;
    const auto vertex = [&](double x, double y) {
        return render::Vertex{static_cast<float>(center_x + x * half_width),
                              static_cast<float>(center_y + y * half_height), 0.5f, 0.5f, color};
    };
    const auto first = static_cast<std::uint32_t>(list.indices_.size());
    const auto base = static_cast<std::uint32_t>(list.vertices_.size());
    if (type == 0) {
        list.vertices_.insert(list.vertices_.end(),
                              {vertex(-1, -1), vertex(1, -1), vertex(1, 1), vertex(-1, 1)});
        list.indices_.insert(list.indices_.end(),
                             {base, base + 1, base + 2, base, base + 2, base + 3});
    } else {
        const auto segments =
                type == 3 ? static_cast<std::uint32_t>(graph::Scalar(node, "sides", 6)) : 128u;
        const auto inner = type == 2 ? graph::Scalar(node, "inner_ratio", 0.75) : 0.0;
        if (inner == 1) return;
        list.vertices_.reserve(list.vertices_.size() + (segments + 1) * 2);
        list.indices_.reserve(list.indices_.size() + segments * 6);
        for (std::uint32_t segment = 0; segment <= segments; ++segment) {
            const auto angle =
                    (static_cast<double>(segment) / segments * 2 - 0.5) * std::numbers::pi;
            const auto x = std::cos(angle), y = std::sin(angle);
            list.vertices_.push_back(vertex(x, y));
            list.vertices_.push_back(vertex(x * inner, y * inner));
            if (segment == segments) continue;
            const auto offset = base + segment * 2;
            list.indices_.insert(list.indices_.end(), {offset, offset + 2, offset + 1});
            if (inner > 0)
                list.indices_.insert(list.indices_.end(), {offset + 1, offset + 2, offset + 3});
        }
    }
    const auto count = static_cast<std::uint32_t>(list.indices_.size()) - first;
    list.commands_.push_back({white, first, count, {0, 0, list.width_, list.height_}});
}
}  // namespace rhythm::runtime::detail
