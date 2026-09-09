#include "scene_selection.h"

#include <algorithm>
#include <stdexcept>

#include "rhythm/runtime/runtime.h"

namespace rhythm::studio {
SceneSelection PickSceneOutput(const graph::Document& document,
                               std::span<const runtime::NodeOutput> outputs, double aspect,
                               double x, double y) {
    SceneSelection result;
    try {
        const auto input = [&](graph::NodeId node, const std::string& port) {
            if (std::any_of(document.bindings_.begin(), document.bindings_.end(),
                            [&](const auto& binding) {
                                return binding.node_ == node && binding.input_ == port;
                            }))
                throw std::invalid_argument("scene_pick.route");
            graph::NodeId source = 0;
            for (const auto& edge : document.edges_)
                if (edge.to_ == node && edge.input_ == port) {
                    if (source) throw std::invalid_argument("scene_pick.route");
                    source = edge.from_;
                }
            return source;
        };
        const auto render = input(document.output_, "source");
        if (!std::any_of(document.nodes_.begin(), document.nodes_.end(), [&](const auto& node) {
                return node.id_ == render && node.type_ == "scene.render";
            }))
            throw std::invalid_argument("scene_pick.route");
        const auto source = input(render, "scene");
        const auto camera_source = input(render, "camera");
        const auto frame = std::find_if(outputs.begin(), outputs.end(),
                                        [&](const auto& output) { return output.node_ == source; });
        if (frame == outputs.end() || !frame->scene_)
            throw std::invalid_argument("canvas.wait_output");
        scene::Camera camera;
        if (camera_source) {
            const auto found =
                    std::find_if(outputs.begin(), outputs.end(),
                                 [&](const auto& output) { return output.node_ == camera_source; });
            if (found == outputs.end() || !found->camera_)
                throw std::invalid_argument("canvas.wait_output");
            camera = *found->camera_;
        }
        const auto ray = scene::CameraRay(camera, aspect, x, y);
        if (!ray) throw std::invalid_argument("scene_pick.ray");
        auto picked = scene::PickScene(*frame->scene_, *ray);
        if (!picked.error_.empty()) throw std::invalid_argument(picked.error_);
        if (!picked.hit_) return result;
        const auto& origin = picked.hit_->origin_;
        const auto selected = origin.transform_ ? origin.transform_ : origin.producer_;
        if (!selected || !std::any_of(document.nodes_.begin(), document.nodes_.end(),
                                      [&](const auto& node) { return node.id_ == selected; }))
            throw std::invalid_argument("scene_pick.component_scope");
        result.selected_ = selected;
        result.hit_ = std::move(picked.hit_);
    } catch (const std::exception& error) {
        result.error_ = error.what();
    }
    return result;
}
}  // namespace rhythm::studio
