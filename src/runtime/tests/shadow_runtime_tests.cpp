#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

#include "rhythm/runtime/viewers.h"
#include "shadow_pass.h"

namespace {
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void StableDirectionalProjection() {
    using namespace rhythm;
    scene::Scene shadow_scene;
    shadow_scene.lights_.push_back({});
    shadow_scene.shadow_ = scene::ShadowSettings{};
    shadow_scene.shadow_->resolution_ = 256;
    shadow_scene.shadow_->extent_ = 10;
    const auto unit = shadow_scene.shadow_->extent_ * 2 / shadow_scene.shadow_->resolution_;
    const auto origin = runtime::detail::ShadowCamera(shadow_scene);
    shadow_scene.shadow_->center_ = {unit * 0.49, unit * 0.49, 0};
    const auto sub_texel = runtime::detail::ShadowCamera(shadow_scene);
    Require(origin.eye_ == sub_texel.eye_ && origin.target_ == sub_texel.target_,
            "sub-grid directional shadow motion keeps the projection stable");
    shadow_scene.shadow_->center_ = {unit * 0.51, unit * 0.51, 0};
    const auto next_texel = runtime::detail::ShadowCamera(shadow_scene);
    Require(std::abs(next_texel.target_.x_ - unit) < 1e-12 &&
                    std::abs(next_texel.target_.y_ - unit) < 1e-12,
            "directional shadow center advances on the Godot stabilization grid");
    shadow_scene.shadow_->center_ = {-unit * 0.51, -unit * 0.51, 0};
    const auto previous_texel = runtime::detail::ShadowCamera(shadow_scene);
    Require(std::abs(previous_texel.target_.x_ + unit) < 1e-12 &&
                    std::abs(previous_texel.target_.y_ + unit) < 1e-12,
            "negative directional motion uses the same stabilization grid");
    shadow_scene.shadow_->center_ = {0, 0, 0.0123};
    const auto depth_motion = runtime::detail::ShadowCamera(shadow_scene);
    Require(std::abs(depth_motion.target_.z_ - 0.0123) < 1e-12,
            "directional shadow depth motion is not quantized");

    shadow_scene.lights_.clear();
    scene::Scene::PositionalLight spot;
    spot.position_ = {0.0123, -0.0456, 3.0789};
    spot.spot_ = true;
    shadow_scene.positional_lights_.push_back(spot);
    shadow_scene.shadow_->light_ = 0;
    const auto spot_camera = runtime::detail::ShadowCamera(shadow_scene);
    Require(spot_camera.eye_ == spot.position_, "spot shadow position is not quantized");
}
void DirectionalCascadeProjection() {
    using namespace rhythm;
    scene::Scene shadow_scene;
    shadow_scene.lights_.push_back({});
    shadow_scene.shadow_ = scene::ShadowSettings{};
    shadow_scene.shadow_->resolution_ = 256;
    shadow_scene.shadow_->cascades_ = 2;
    shadow_scene.shadow_->cascade_split_ = 0.25;
    shadow_scene.shadow_->max_distance_ = 41;
    scene::Camera receiver;
    receiver.near_ = 1;
    receiver.far_ = 101;
    const auto cascades = runtime::detail::CascadeCameras(shadow_scene, receiver, 1);
    Require(std::abs(cascades.split_depth_ - 11) < 1e-12,
            "directional cascade split uses bounded receiver view depth");
    Require(cascades.cameras_[0].kind_ == scene::ProjectionKind::kOrthographic &&
                    cascades.cameras_[1].kind_ == scene::ProjectionKind::kOrthographic &&
                    cascades.cameras_[0].orthographic_height_ <
                            cascades.cameras_[1].orthographic_height_,
            "directional cascades fit increasing frustum segments");
    const auto unit = cascades.cameras_[0].orthographic_height_ * 2 / 256;
    receiver.eye_.x_ += unit * 0.49;
    receiver.target_.x_ += unit * 0.49;
    const auto stable = runtime::detail::CascadeCameras(shadow_scene, receiver, 1);
    Require(cascades.cameras_[0].eye_ == stable.cameras_[0].eye_ &&
                    cascades.cameras_[0].target_ == stable.cameras_[0].target_,
            "first directional cascade remains stable under sub-grid camera motion");
    receiver.near_ = 50;
    const auto clamped = runtime::detail::CascadeCameras(shadow_scene, receiver, 1);
    Require(clamped.split_depth_ > receiver.near_ && clamped.split_depth_ < receiver.near_ + 0.001,
            "cascade max distance below the camera near plane keeps a finite depth interval");
}
void DirectionalCascadeResources() {
    using namespace rhythm;
    graph::Registry registry;
    graph::Document document;
    document.id_ = "shadow.cascade.runtime";
    document.nodes_ = {registry.MakeNode(1, "scene.directional_light"),
                       registry.MakeNode(2, "scene.shadow"), registry.MakeNode(3, "scene.render"),
                       registry.MakeNode(4, "output.texture")};
    document.nodes_[1].properties_["shadow_resolution"] = 0.0;
    document.edges_ = {{1, 1, 2, "scene"}, {2, 2, 3, "scene"}, {3, 3, 4, "source"}};
    document.output_ = 4;
    runtime::Runtime runtime;
    auto renderer = render::Renderer::CreateNull();
    const auto evaluate = [&] {
        const auto plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
        renderer.BeginFrame();
        const auto result = runtime.Evaluate(plan, {0, 0, {64, 32}}, renderer);
        renderer.EndFrame();
        return result;
    };
    (void)evaluate();
    const auto single_bytes = renderer.Stats().texture_bytes_;
    const auto single_textures = renderer.Stats().live_textures_;
    document.nodes_[1].properties_["shadow_cascades"] = 1.0;
    (void)evaluate();
    Require(renderer.Stats().texture_bytes_ == single_bytes + 256 * 256 * 8 &&
                    renderer.Stats().live_textures_ == single_textures + 2,
            "two cascades own exactly one additional bounded depth/color pair");
    runtime.Reset();
    Require(renderer.Stats().live_textures_ == 0,
            "directional cascade attachments release on reset");
}
void Run() {
    using namespace rhythm;
    StableDirectionalProjection();
    DirectionalCascadeProjection();
    DirectionalCascadeResources();
    graph::Registry registry;
    graph::Document document;
    document.id_ = "shadow.runtime";
    document.nodes_ = {registry.MakeNode(1, "geometry.cube"),
                       registry.MakeNode(2, "scene.instance"),
                       registry.MakeNode(3, "scene.spot_light"),
                       registry.MakeNode(4, "scene.shadow"),
                       registry.MakeNode(5, "scene.directional_light"),
                       registry.MakeNode(6, "scene.merge"),
                       registry.MakeNode(7, "scene.merge"),
                       registry.MakeNode(8, "scene.render"),
                       registry.MakeNode(9, "output.texture")};
    document.edges_ = {{1, 1, 2, "geometry"}, {2, 3, 4, "scene"}, {3, 4, 6, "a"},
                       {4, 5, 6, "b"},        {5, 2, 7, "a"},     {6, 6, 7, "b"},
                       {7, 7, 8, "scene"},    {8, 8, 9, "source"}};
    document.nodes_[3].properties_["shadow_resolution"] = 0.0;
    document.output_ = 9;
    runtime::Runtime runtime;
    runtime::Viewers viewers;
    auto renderer = render::Renderer::CreateNull();
    const auto evaluate = [&] {
        const auto plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
        renderer.BeginFrame();
        const auto result = runtime.Evaluate(plan, {0, 0, {64, 32}}, renderer);
        renderer.EndFrame();
        return result;
    };
    const auto first = evaluate();
    const auto output = [&](const runtime::FrameResult& result,
                            graph::NodeId id) -> const runtime::NodeOutput& {
        return *std::find_if(result.outputs_.begin(), result.outputs_.end(),
                             [id](const auto& value) { return value.node_ == id; });
    };
    Require(output(first, 4).scene_->shadow_->light_ == 0 &&
                    output(first, 7).scene_->shadow_->light_ == 1,
            "merging a directional light preserves the selected positional-light identity");
    Require(output(first, 4).scene_->shadow_->filter_ == scene::ShadowFilter::kPcf5,
            "legacy default value keeps PCF5 filtering");
    Require(output(first, 4).scene_->shadow_->cascades_ == 1,
            "legacy shadow nodes keep a single projection");
    std::cout << "shadow draws=" << renderer.Stats().draws_
              << " textures=" << renderer.Stats().live_textures_ << '\n';
    Require(renderer.Stats().draws_ == 2 && renderer.Stats().live_textures_ == 4,
            "shadow scene uses one receiver, a shadow pair and the shared white texel, with two "
            "draws");
    const auto bytes = renderer.Stats().texture_bytes_;
    Require(evaluate().evaluated_ == 0 && renderer.Stats().texture_bytes_ == bytes,
            "unchanged shadow scene does not redraw or reallocate");
    document.nodes_[3].properties_["shadow_filter"] = 2.0;
    Require(output(evaluate(), 4).scene_->shadow_->filter_ == scene::ShadowFilter::kPcf13,
            "high shadow quality reaches the runtime scene");
    document.nodes_[3].properties_["shadow_filter"] = 0.0;
    Require(output(evaluate(), 4).scene_->shadow_->filter_ == scene::ShadowFilter::kNearest,
            "legacy disabled filter value maps to nearest sampling");
    document.nodes_[3].properties_["shadow_filter"] = 1.0;
    renderer.BeginFrame();
    viewers.BeginFrame(0, true, 0);
    const std::array<graph::NodeId, 1> demand{7};
    viewers.Capture(first, demand, renderer);
    Require(viewers.Outputs().size() == 1 && renderer.Stats().live_textures_ == 8,
            "shadowed scene preview owns an independent bounded depth pair");
    viewers.BeginFrame(1, false, 0);
    Require(renderer.Stats().live_textures_ == 4,
            "hidden shadow previews release their depth pair");
    renderer.EndFrame();
    document.nodes_[3].properties_["shadow_resolution"] = 1.0;
    (void)evaluate();
    Require(renderer.Stats().live_textures_ == 4 &&
                    renderer.Stats().texture_bytes_ == bytes + (512 * 512 - 256 * 256) * 8,
            "resolution replaces the pair and accounts for both attachments");
    document.nodes_[3].properties_["shadow_enabled"] = 0.0;
    (void)evaluate();
    Require(renderer.Stats().live_textures_ == 2 && renderer.Stats().draws_ == 1,
            "disabling shadows releases extra attachments and the caster pass");
    runtime.Reset();
    Require(renderer.Stats().live_textures_ == 0 && renderer.Stats().live_meshes_ == 0,
            "all shadow and geometry resources release on reset");
}
}  // namespace
int main() {
    try {
        Run();
        std::cout << "Shadow runtime: merge identity, cache, previews, resolution and release "
                     "passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
