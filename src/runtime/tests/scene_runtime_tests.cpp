#include <algorithm>
#include <iostream>
#include <stdexcept>

#include "rhythm/runtime/viewers.h"

namespace {
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void Run() {
    using namespace rhythm;
    graph::Registry registry;
    graph::Document document;
    document.id_ = "scene.runtime";
    document.nodes_ = {
            registry.MakeNode(1, "geometry.cube"),  registry.MakeNode(2, "material.unlit"),
            registry.MakeNode(3, "scene.instance"), registry.MakeNode(4, "scene.transform"),
            registry.MakeNode(5, "scene.render"),   registry.MakeNode(6, "output.texture")};
    document.edges_ = {{1, 1, 3, "geometry"},
                       {2, 2, 3, "material"},
                       {3, 3, 4, "scene"},
                       {4, 4, 5, "scene"},
                       {5, 5, 6, "source"}};
    document.output_ = 6;
    auto renderer = render::Renderer::CreateNull();
    runtime::Runtime runtime;
    std::shared_ptr<const scene::Resources> resources;
    auto evaluate = [&] {
        const auto plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
        renderer.BeginFrame();
        runtime::FrameContext context{0, 0, {640, 360}};
        context.resources_ = resources;
        const auto frame = runtime.Evaluate(plan, context, renderer);
        renderer.EndFrame();
        return frame;
    };
    auto frame = evaluate();
    const auto geometry = frame.outputs_[0].geometry_;
    Require(renderer.IsValid(frame.final_) && renderer.Stats().live_meshes_ == 1,
            "typed geometry to scene to texture reaches render backend");
    const auto bytes = renderer.Stats().mesh_bytes_;
    Require(evaluate().evaluated_ == 0, "unchanged scene is cached");
    document.nodes_[3].properties_["rotation_y"] = 45.0;
    document.nodes_[3].properties_["translate_x"] = 0.5;
    frame = evaluate();
    Require(frame.outputs_[0].geometry_ == geometry && renderer.Stats().mesh_bytes_ == bytes &&
                    renderer.Stats().live_meshes_ == 1,
            "transform updates share geometry and retain GPU upload");
    Require(frame.outputs_[3].scene_->instances_.at(0).transform_.values_[12] == 0.5 &&
                    frame.outputs_[2].scene_->instances_.at(0).transform_ == scene::Matrix{},
            "transform publishes an immutable scene snapshot");
    document.nodes_[0] = registry.MakeNode(1, "geometry.sphere");
    frame = evaluate();
    Require(frame.outputs_[0].geometry_->revision_ != geometry->revision_ &&
                    renderer.Stats().live_meshes_ == 1 && renderer.Stats().mesh_bytes_ > bytes,
            "same node ID changing geometry type invalidates upload and downstream snapshots");
    runtime::Viewers viewers;
    renderer.BeginFrame();
    viewers.BeginFrame(0, true, 0);
    const std::array<graph::NodeId, 2> demand{1, 4};
    viewers.Capture(frame, demand, renderer);
    Require(viewers.Outputs().size() == 2 && renderer.Stats().live_meshes_ == 3,
            "geometry and scene have inline GPU previews");
    viewers.BeginFrame(1, false, 0);
    Require(viewers.Outputs().empty() && renderer.Stats().live_meshes_ == 1,
            "hidden previews release all preview geometry");
    renderer.EndFrame();
    runtime.Reset();
    Require(renderer.Stats().live_meshes_ == 0 && renderer.Stats().live_textures_ == 0,
            "runtime reset releases scene mesh and depth target owners");
    frame = evaluate();
    Require(renderer.IsValid(frame.final_) && renderer.Stats().live_meshes_ == 1,
            "runtime reconstructs resources after reset");
    document.nodes_[1] = registry.MakeNode(2, "material.pbr");
    frame = evaluate();
    Require(frame.outputs_[1].material_ && !frame.outputs_[1].material_->unlit_,
            "PBR material reaches scene runtime");
    renderer.BeginFrame();
    viewers.BeginFrame(2, true, 0);
    const std::array<graph::NodeId, 1> material_demand{2};
    viewers.Capture(frame, material_demand, renderer);
    Require(viewers.Outputs().size() == 1 && renderer.Stats().live_meshes_ == 2,
            "material preview owns bounded sphere geometry");
    viewers.BeginFrame(3, false, 0);
    Require(renderer.Stats().live_meshes_ == 1, "hiding material preview releases GPU sphere");
    renderer.EndFrame();
    const assets::AssetId asset_id{std::string(64, 'a')};
    scene::Resources catalog;
    catalog.models_.push_back(scene::DescribeModel(asset_id, scene::Cube()));
    resources = std::make_shared<const scene::Resources>(std::move(catalog));
    document.nodes_[0] = registry.MakeNode(1, "geometry.glb");
    document.nodes_[0].properties_["asset"] = asset_id;
    frame = evaluate();
    Require(frame.outputs_[0].geometry_->model_ == resources->models_[0].model_,
            "asset geometry shares prepared immutable CPU model without parsing during evaluation");
    const auto resolved = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
    Require(graph::ValidateSceneBudget(resolved, std::span<const graph::GeometryBudget>{})
                    .has_value(),
            "resolved budget validation rejects an absent imported resource");
    std::cout << "Scene runtime: cache, immutable instances, replacement, previews and resource "
                 "cleanup passed\n";
}
}  // namespace
int main() {
    try {
        Run();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
