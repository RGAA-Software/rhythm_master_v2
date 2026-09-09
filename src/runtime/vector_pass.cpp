#include "vector_pass.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace rhythm::runtime::detail {
namespace {
geometry2d::Contour Project(const scene::Path& path, render::Extent extent, double span,
                            std::size_t samples, int plane) {
    scene::Validate(path);
    geometry2d::Contour contour;
    contour.closed_ = path.closed_;
    const auto count = std::min(samples, path.points_.size());
    const auto scale = std::min(extent.width_, extent.height_) / span;
    for (std::size_t index = 0; index < count; ++index) {
        const auto source = count < 2      ? 0
                            : path.closed_ ? index * path.points_.size() / count
                                           : index * (path.points_.size() - 1) / (count - 1);
        const auto point = path.points_[source];
        const auto x = plane == 2 ? point.y_ : point.x_;
        const auto y = plane == 0 ? point.y_ : point.z_;
        contour.points_.push_back(
                {extent.width_ * .5 + x * scale, extent.height_ * .5 - y * scale});
    }
    return contour;
}
std::uint32_t Pack(graph::Color color) {
    const auto channel = [](double value) {
        return static_cast<std::uint32_t>(std::clamp(value, 0.0, 1.0) * 255 + .5);
    };
    return channel(color.r_) | channel(color.g_) << 8 | channel(color.b_) << 16 |
           channel(color.a_) << 24;
}
}  // namespace
void VectorMeshes::Retain(const graph::ExecutionPlan& plan) {
    std::set<graph::NodeId> required;
    for (const auto& instruction : plan.instructions_)
        if (instruction.operation_ == graph::Operation::kVectorFill ||
            instruction.operation_ == graph::Operation::kVectorStroke)
            required.insert(instruction.node_.id_);
    if (required.size() > 64) throw std::length_error("vector.node_budget");
    std::erase_if(meshes_, [&](const auto& entry) { return !required.contains(entry.first); });
}
void VectorMeshes::Draw(const graph::Instruction& instruction, std::span<const NodeOutput> outputs,
                        render::TextureHandle white, render::DrawList& list) {
    const auto& node = instruction.node_;
    const auto input = [&](std::size_t port) -> const NodeOutput& {
        if (port >= instruction.inputs_.size() || !instruction.inputs_[port] ||
            *instruction.inputs_[port] >= outputs.size())
            throw std::invalid_argument("vector.input");
        return outputs[*instruction.inputs_[port]];
    };
    if (!input(0).path_) throw std::invalid_argument("vector.path");
    Key key;
    key.operation_ = instruction.operation_;
    if (!std::isfinite(list.width_) || !std::isfinite(list.height_) || list.width_ < 1 ||
        list.height_ < 1 || list.width_ > 65535 || list.height_ > 65535)
        throw std::invalid_argument("vector.extent");
    key.extent_ = {static_cast<std::uint16_t>(list.width_),
                   static_cast<std::uint16_t>(list.height_)};
    key.sources_ = {input(0).node_, input(0).version_, 0, 0};
    const bool fill = instruction.operation_ == graph::Operation::kVectorFill;
    const bool hole = fill && instruction.inputs_.size() > 1 && instruction.inputs_[1];
    if (hole) {
        if (!input(1).path_) throw std::invalid_argument("vector.hole");
        key.sources_[2] = input(1).node_;
        key.sources_[3] = input(1).version_;
    }
    const auto scalar = [&](std::string_view name, double fallback) {
        return graph::Scalar(node, name, fallback);
    };
    auto width = scalar("vector_width", 4);
    if (!fill && instruction.inputs_.size() > 1 && instruction.inputs_[1]) {
        const auto value = input(1).scalar_;
        width = std::isfinite(value) ? std::clamp(value, 0.0, 100.0) : width;
    }
    key.layout_ = {scalar("path_span", 4),    scalar("path_samples", 512),
                   scalar("path_plane", 0),   width,
                   scalar("vector_join", 2),  scalar("vector_cap", 2),
                   scalar("vector_miter", 4), scalar("vector_tolerance", .25)};
    if (!meshes_.contains(node.id_) && meshes_.size() >= 64)
        throw std::length_error("vector.node_budget");
    auto& entry = meshes_[node.id_];
    if (!entry.key_ || *entry.key_ != key) {
        std::vector<geometry2d::Contour> contours;
        contours.push_back(Project(*input(0).path_, key.extent_, key.layout_[0],
                                   static_cast<std::size_t>(key.layout_[1]),
                                   static_cast<int>(key.layout_[2])));
        if (hole)
            contours.push_back(Project(*input(1).path_, key.extent_, key.layout_[0],
                                       static_cast<std::size_t>(key.layout_[1]),
                                       static_cast<int>(key.layout_[2])));
        geometry2d::StrokeStyle style;
        style.width_ = width;
        style.join_ = static_cast<geometry2d::LineJoin>(static_cast<int>(key.layout_[4]));
        style.cap_ = static_cast<geometry2d::LineCap>(static_cast<int>(key.layout_[5]));
        style.miter_limit_ = key.layout_[6];
        style.arc_tolerance_ = key.layout_[7];
        auto mesh = fill ? geometry2d::FillContours(contours)
                         : geometry2d::StrokeContours(contours, style);
        std::size_t vertices = mesh.vertices_.size(), indices = mesh.indices_.size();
        for (const auto& [id, other] : meshes_)
            if (id != node.id_) {
                vertices += other.mesh_.vertices_.size();
                indices += other.mesh_.indices_.size();
            }
        if (vertices > 65536 || indices > 196608) throw std::length_error("vector.mesh_budget");
        entry.mesh_ = std::move(mesh);
        entry.key_ = key;
        ++builds_;
    }
    if (entry.mesh_.indices_.empty()) return;
    const auto color = Pack(graph::ColorValue(node, "color_a", {1, 1, 1, 1}));
    const auto base = static_cast<std::uint32_t>(list.vertices_.size());
    const auto first = static_cast<std::uint32_t>(list.indices_.size());
    for (const auto point : entry.mesh_.vertices_)
        list.vertices_.push_back(
                {static_cast<float>(point.x_), static_cast<float>(point.y_), .5f, .5f, color});
    for (const auto index : entry.mesh_.indices_) list.indices_.push_back(base + index);
    list.commands_.push_back({white,
                              first,
                              static_cast<std::uint32_t>(entry.mesh_.indices_.size()),
                              {0, 0, list.width_, list.height_}});
}
}  // namespace rhythm::runtime::detail
