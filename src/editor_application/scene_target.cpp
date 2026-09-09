#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

#include "rhythm/editor/scene_edit.h"

namespace rhythm::editor {
namespace {
double StaticScalar(const graph::Node& node, const std::string& key, double fallback) {
    const auto found = node.properties_.find(key);
    if (found == node.properties_.end()) return fallback;
    if (!std::holds_alternative<double>(found->second) ||
        !std::isfinite(std::get<double>(found->second)))
        throw std::invalid_argument("scene_edit.driven");
    return std::get<double>(found->second);
}
scene::EulerPose Pose(const graph::Document& document, const graph::Node& node) {
    for (const auto& edge : document.edges_)
        if (edge.to_ == node.id_ && edge.input_ != "scene")
            throw std::invalid_argument("scene_edit.driven");
    for (const auto& binding : document.bindings_)
        if (binding.node_ == node.id_ && binding.input_ != "scene")
            throw std::invalid_argument("scene_edit.driven");
    const auto bounded = [&](const std::string& key, double fallback, double minimum,
                             double maximum) {
        const auto value = StaticScalar(node, key, fallback);
        if (value < minimum || value > maximum) throw std::invalid_argument("scene_edit.range");
        return value;
    };
    const auto scale = bounded("scale", 1, .001, 100);
    const auto axis = [&](const std::string& key) {
        return std::clamp(scale * bounded(key, 1, .001, 100), .001, 100.0);
    };
    return {{bounded("translate_x", 0, -1000, 1000), bounded("translate_y", 0, -1000, 1000),
             bounded("translate_z", 0, -1000, 1000)},
            {bounded("rotation_x", 0, -36000, 36000), bounded("rotation_y", 0, -36000, 36000),
             bounded("rotation_z", 0, -36000, 36000)},
            {axis("scale_x"), axis("scale_y"), axis("scale_z")}};
}
scene::Camera Camera(const graph::Node& node) {
    if (node.type_ != "scene.camera") throw std::invalid_argument("scene_edit.camera_scope");
    scene::Camera result;
    result.eye_ = {StaticScalar(node, "eye_x", 0), StaticScalar(node, "eye_y", 0),
                   StaticScalar(node, "eye_z", 3)};
    result.target_ = {StaticScalar(node, "target_x", 0), StaticScalar(node, "target_y", 0),
                      StaticScalar(node, "target_z", 0)};
    result.kind_ = StaticScalar(node, "projection", 0) == 0 ? scene::ProjectionKind::kPerspective
                                                            : scene::ProjectionKind::kOrthographic;
    result.vertical_fov_ = StaticScalar(node, "field_of_view", 60);
    result.orthographic_height_ = StaticScalar(node, "orthographic_height", 2);
    result.near_ = StaticScalar(node, "near_plane", .05);
    result.far_ = StaticScalar(node, "far_plane", 1000);
    (void)scene::View(result);
    (void)scene::Projection(result, 1);
    return result;
}
}  // namespace
SceneInspection InspectSceneTarget(const Snapshot& snapshot, graph::NodeId selected) {
    try {
        const auto& document = snapshot.document_;
        if (!graph::ValidCanvas(document.canvas_))
            throw std::invalid_argument("scene_edit.invalid_graph");
        std::map<graph::NodeId, std::size_t> nodes;
        for (std::size_t index = 0; index < document.nodes_.size(); ++index)
            if (!nodes.emplace(document.nodes_[index].id_, index).second)
                throw std::invalid_argument("scene_edit.invalid_graph");
        if (!nodes.contains(selected) ||
            document.nodes_[nodes.at(selected)].type_ != "scene.transform")
            throw std::invalid_argument("scene_edit.select_transform");
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
            for (const auto edge : incoming[id]) pending.push_back(document.edges_[edge].from_);
        }
        if (!reachable.contains(selected)) throw std::invalid_argument("scene_edit.not_in_output");
        SceneTarget result;
        result.node_ = selected;
        result.pose_ = Pose(document, document.nodes_[nodes.at(selected)]);
        result.uniform_scale_ = StaticScalar(document.nodes_[nodes.at(selected)], "scale", 1);
        if (result.uniform_scale_ < .001 || result.uniform_scale_ > 100)
            throw std::invalid_argument("scene_edit.range");
        std::set<graph::NodeId> visited;
        auto id = selected;
        while (id != document.output_) {
            if (!visited.insert(id).second) throw std::invalid_argument("scene_edit.invalid_graph");
            std::optional<std::size_t> next;
            for (const auto edge : outgoing[id])
                if (reachable.contains(document.edges_[edge].to_)) {
                    if (next) throw std::invalid_argument("scene_edit.ambiguous_route");
                    next = edge;
                }
            if (!next) throw std::invalid_argument("scene_edit.not_in_output");
            const auto& edge = document.edges_[*next];
            id = edge.to_;
            if (!nodes.contains(id)) throw std::invalid_argument("scene_edit.invalid_graph");
            const auto& node = document.nodes_[nodes.at(id)];
            if (!result.render_node_) {
                if (node.type_ == "scene.transform" && edge.input_ == "scene")
                    result.parent_ = scene::Multiply(scene::ComposeEuler(Pose(document, node)),
                                                     result.parent_);
                else if (node.type_ == "scene.merge" &&
                         (edge.input_ == "a" || edge.input_ == "b")) {
                    // Scene merge preserves each instance's coordinates.
                } else if (node.type_ == "scene.render" && edge.input_ == "scene") {
                    result.render_node_ = id;
                    result.scene_source_ = edge.from_;
                    for (const auto input : incoming[id]) {
                        const auto& camera_edge = document.edges_[input];
                        if (camera_edge.input_ != "camera") continue;
                        if (!nodes.contains(camera_edge.from_))
                            throw std::invalid_argument("scene_edit.invalid_graph");
                        result.camera_ = Camera(document.nodes_[nodes.at(camera_edge.from_)]);
                    }
                    for (const auto& binding : document.bindings_)
                        if (binding.node_ == id && binding.input_ == "camera")
                            throw std::invalid_argument("scene_edit.camera_scope");
                } else {
                    throw std::invalid_argument("scene_edit.unsupported_route");
                }
            } else if (!(node.type_ == "output.texture" && edge.input_ == "source")) {
                throw std::invalid_argument("scene_edit.image_warp");
            }
        }
        if (!result.render_node_) throw std::invalid_argument("scene_edit.unsupported_route");
        (void)scene::InverseAffine(result.parent_);
        (void)scene::InverseAffine(scene::ComposeEuler(result.pose_));
        return result;
    } catch (const std::exception& error) {
        return graph::Diagnostic{error.what(), selected, {}};
    }
}
}  // namespace rhythm::editor
