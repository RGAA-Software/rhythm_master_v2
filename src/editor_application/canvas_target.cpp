#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

#include "rhythm/editor/canvas_edit.h"

namespace rhythm::editor {
namespace {
geometry2d::Pose Pose(const graph::Document& document, const graph::Node& node) {
    // Even opacity is checked: a hidden/driven target cannot be represented
    // accurately by a static author-space outline.
    for (const auto& edge : document.edges_)
        if (edge.to_ == node.id_ && edge.input_ != "source")
            throw std::invalid_argument("canvas.driven_transform");
    for (const auto& binding : document.bindings_)
        if (binding.node_ == node.id_ && binding.input_ != "source")
            throw std::invalid_argument("canvas.driven_transform");
    const auto scalar = [&](std::string_view key, double fallback) {
        const auto found = node.properties_.find(std::string(key));
        if (found == node.properties_.end()) return fallback;
        if (!std::holds_alternative<double>(found->second) ||
            !std::isfinite(std::get<double>(found->second)))
            throw std::invalid_argument("canvas.driven_transform");
        return std::get<double>(found->second);
    };
    if (scalar("opacity", 1) <= 0) throw std::invalid_argument("canvas.invisible");
    const auto scale = scalar("scale", 1);
    geometry2d::Pose result{{scalar("translate_x", 0), scalar("translate_y", 0)},
                            {scalar("pivot_x", .5), scalar("pivot_y", .5)},
                            {scale * scalar("scale_x", 1), scale * scalar("scale_y", 1)},
                            scalar("rotation", 0)};
    if (scale < 0 || scale > 8 || std::abs(scalar("scale_x", 1)) > 8 ||
        std::abs(scalar("scale_y", 1)) > 8 || std::abs(result.translation_.x_) > 4 ||
        std::abs(result.translation_.y_) > 4 || std::abs(result.degrees_) > 36000 ||
        result.pivot_.x_ < 0 || result.pivot_.x_ > 1 || result.pivot_.y_ < 0 ||
        result.pivot_.y_ > 1 || scalar("opacity", 1) > 1)
        throw std::invalid_argument("canvas.invalid_property");
    return result;
}
}  // namespace
CanvasInspection InspectCanvasTarget(const Snapshot& snapshot, graph::NodeId selected) {
    const auto& document = snapshot.document_;
    try {
        if (!graph::ValidCanvas(document.canvas_))
            throw std::invalid_argument("canvas.invalid_extent");
        std::map<graph::NodeId, std::size_t> indices;
        for (std::size_t index = 0; index < document.nodes_.size(); ++index)
            if (!indices.emplace(document.nodes_[index].id_, index).second)
                throw std::invalid_argument("canvas.invalid_graph");
        if (!indices.contains(selected) ||
            document.nodes_[indices.at(selected)].type_ != "texture.affine")
            throw std::invalid_argument("canvas.select_affine");
        const auto& node = document.nodes_[indices.at(selected)];
        CanvasTarget target;
        target.node_ = selected;
        target.canvas_ = {double(document.canvas_.width_), double(document.canvas_.height_)};
        target.pose_ = Pose(document, node);
        target.uniform_scale_ = graph::Scalar(node, "scale", 1);
        if (!geometry2d::Inverse(geometry2d::Compose(target.pose_, target.canvas_)))
            throw std::invalid_argument("canvas.singular_transform");
        std::map<graph::NodeId, std::vector<std::size_t>> incoming, outgoing;
        for (std::size_t index = 0; index < document.edges_.size(); ++index) {
            const auto& edge = document.edges_[index];
            incoming[edge.to_].push_back(index);
            outgoing[edge.from_].push_back(index);
        }
        std::set<graph::NodeId> reachable;
        std::vector<graph::NodeId> pending{document.output_};
        while (!pending.empty()) {
            const auto id = pending.back();
            pending.pop_back();
            if (!reachable.insert(id).second) continue;
            for (const auto index : incoming[id]) pending.push_back(document.edges_[index].from_);
        }
        if (!reachable.contains(selected)) throw std::invalid_argument("canvas.not_in_output");
        std::set<graph::NodeId> visited;
        auto id = selected;
        while (id != document.output_) {
            if (!visited.insert(id).second) throw std::invalid_argument("canvas.invalid_graph");
            std::optional<std::size_t> next;
            for (const auto index : outgoing[id])
                if (reachable.contains(document.edges_[index].to_)) {
                    if (next) throw std::invalid_argument("canvas.ambiguous_route");
                    next = index;
                }
            if (!next) throw std::invalid_argument("canvas.not_in_output");
            const auto& edge = document.edges_[*next];
            id = edge.to_;
            if (!indices.contains(id)) throw std::invalid_argument("canvas.invalid_graph");
            const auto& parent = document.nodes_[indices.at(id)];
            if (parent.type_ == "texture.affine" && edge.input_ == "source")
                target.parent_ = geometry2d::Multiply(
                        geometry2d::Compose(Pose(document, parent), target.canvas_),
                        target.parent_);
            else if (!(parent.type_ == "output.texture" && edge.input_ == "source") &&
                     !(parent.type_ == "texture.composite" &&
                       (edge.input_ == "a" || edge.input_ == "b")))
                throw std::invalid_argument("canvas.unsupported_route");
        }
        if (!geometry2d::Inverse(target.parent_))
            throw std::invalid_argument("canvas.singular_parent");
        return target;
    } catch (const std::exception& error) {
        return graph::Diagnostic{error.what(), selected, {}};
    }
}
}  // namespace rhythm::editor
