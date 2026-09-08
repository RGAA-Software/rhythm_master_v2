#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

#include "rhythm/runtime/viewers.h"

namespace {
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void Lights() {
    using namespace rhythm;
    graph::Registry registry;
    for (bool spot : {false, true}) {
        graph::Document document;
        document.id_ = "scene.lights";
        document.nodes_ = {
                registry.MakeNode(1, spot ? "scene.spot_light" : "scene.point_light"),
                registry.MakeNode(2, "scene.transform"), registry.MakeNode(3, "scene.render"),
                registry.MakeNode(4, "output.texture"), registry.MakeNode(5, "scalar.constant")};
        document.nodes_[0].properties_["translate_x"] = 1.0;
        document.nodes_[0].properties_["translate_y"] = 0.0;
        if (spot) {
            document.nodes_[0].properties_["light_x"] = 1.0;
            document.nodes_[0].properties_["light_y"] = 0.0;
            document.nodes_[0].properties_["light_z"] = 0.0;
        }
        document.nodes_[1].properties_["rotation_z"] = 90.0;
        document.nodes_[1].properties_["scale"] = 2.0;
        document.nodes_[1].properties_["translate_x"] = 3.0;
        document.nodes_[4].properties_["value"] = 12.0;
        document.edges_ = {{1, 1, 2, "scene"},
                           {2, 2, 3, "scene"},
                           {3, 3, 4, "source"},
                           {4, 5, 1, "light_energy"}};
        if (spot) document.edges_.push_back({5, 5, 1, "spot_angle"});
        document.output_ = 4;
        auto renderer = render::Renderer::CreateNull();
        runtime::Runtime runtime;
        const auto plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
        renderer.BeginFrame();
        const auto frame = runtime.Evaluate(plan, {0, 0, {64, 64}}, renderer);
        const auto find = [&](graph::NodeId id) -> const scene::Scene& {
            return *std::find_if(frame.outputs_.begin(), frame.outputs_.end(),
                                 [id](const auto& item) { return item.node_ == id; })
                            ->scene_;
        };
        const auto& source = find(1).positional_lights_.at(0);
        const auto& transformed = find(2).positional_lights_.at(0);
        Require(source.radiance_.x_ == 12 && source.position_.x_ == 1 &&
                        transformed.range_ == source.range_,
                "wired energy and world-unit range survive immutable scene transform");
        Require(std::abs(transformed.position_.x_ - 3) < 1e-6 &&
                        std::abs(transformed.position_.y_ - 2) < 1e-6 &&
                        std::abs(transformed.position_.z_ - 6) < 1e-6,
                "positional light follows scene scale rotation and translation");
        if (spot)
            Require(std::abs(transformed.direction_.y_ - 1) < 1e-6 && transformed.cone_angle_ == 12,
                    "spot axis rotates and a wired cone angle overrides its property");
        runtime::Viewers viewers;
        viewers.BeginFrame(0, true, 0);
        const std::array<graph::NodeId, 1> demand{1};
        viewers.Capture(frame, demand, renderer);
        Require(viewers.Outputs().size() == 1 && renderer.Stats().live_meshes_ == 1,
                "a local-light-only node gets bounded sphere preview geometry");
        renderer.EndFrame();
    }
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
    document.nodes_[3].properties_["scale"] = 2.0;
    document.nodes_[3].properties_["scale_x"] = 0.25;
    document.nodes_[3].properties_["scale_y"] = 3.0;
    document.nodes_[3].properties_["scale_z"] = 0.5;
    frame = evaluate();
    const auto& matrix = frame.outputs_[3].scene_->instances_.at(0).transform_;
    const auto expected = scene::Multiply(
            scene::Compose({0.5, 0, 0},
                           {0, std::sin(std::acos(-1.0) / 8), 0, std::cos(std::acos(-1.0) / 8)},
                           {1, 1, 1}),
            scene::Compose({}, {0, 0, 0, 1}, {0.5, 6, 1}));
    for (std::size_t index = 0; index < matrix.values_.size(); ++index)
        Require(std::abs(matrix.values_[index] - expected.values_[index]) < 1e-6,
                "axis scaling precedes rotation and translation");
    const auto normal = scene::NormalTransform(matrix);
    Require(std::abs(normal.values_[5] - 1.0 / 6) < 1e-6 &&
                    frame.outputs_[0].geometry_ == geometry &&
                    renderer.Stats().mesh_bytes_ == bytes,
            "nonuniform scaling preserves inverse-transpose normals and shared geometry");
    document.nodes_.push_back(registry.MakeNode(7, "scalar.constant"));
    document.nodes_.back().properties_["value"] = 0.75;
    document.edges_.push_back({6, 7, 4, "scale_y"});
    frame = evaluate();
    const auto transformed = std::find_if(frame.outputs_.begin(), frame.outputs_.end(),
                                          [](const auto& item) { return item.node_ == 4; });
    Require(transformed != frame.outputs_.end() &&
                    std::abs(transformed->scene_->instances_[0].transform_.values_[5] - 1.5) < 1e-6,
            "wired axis overrides the property and multiplies uniform scale");
    document.edges_.pop_back();
    document.nodes_.pop_back();
    document.nodes_[3].properties_["scale"] = 0.001;
    document.nodes_[3].properties_["scale_x"] = 0.001;
    document.nodes_[3].properties_["scale_y"] = 0.001;
    document.nodes_[3].properties_["scale_z"] = 0.001;
    frame = evaluate();
    Require(scene::ValidAffine(
                    scene::NormalTransform(frame.outputs_[3].scene_->instances_[0].transform_)),
            "minimum axis products retain an invertible transform");
    document.nodes_[3] = registry.MakeNode(4, "scene.transform");
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
    auto textured = scene::Cube();
    textured.images_.push_back({1, 1, {128, 128, 255, 255}});
    textured.materials_[0].unlit_ = false;
    textured.materials_[0].textures_.images_.fill(0u);
    const assets::AssetId textured_id{std::string(64, 'b')};
    document.nodes_[0].properties_["asset"] = textured_id;
    scene::Resources textured_catalog;
    textured_catalog.models_.push_back(scene::DescribeModel(textured_id, std::move(textured)));
    resources = std::make_shared<const scene::Resources>(std::move(textured_catalog));
    document.edges_.erase(document.edges_.begin() +
                          1);  // Use imported material instead of graph override.
    frame = evaluate();
    std::cout << "model image textures=" << renderer.Stats().live_textures_
              << " cpu_bytes=" << resources->models_[0].image_bytes_
              << " outputs=" << frame.outputs_.size() << "\n";
    Require(renderer.Stats().live_textures_ == 3 && resources->models_[0].image_bytes_ == 4,
            "one embedded image shared by four slots uploads once with bounded CPU accounting");
    const auto textured_bytes = renderer.Stats().texture_bytes_;
    document.nodes_[3].properties_["rotation_y"] = 40.0;
    frame = evaluate();
    Require(renderer.Stats().texture_bytes_ == textured_bytes &&
                    renderer.Stats().live_textures_ == 3,
            "model image uploads are reused across animation");
    renderer.BeginFrame();
    viewers.BeginFrame(4, true, 0);
    const std::array<graph::NodeId, 1> image_demand{1};
    viewers.Capture(frame, image_demand, renderer);
    Require(viewers.Outputs().size() == 1 && renderer.Stats().live_textures_ == 6,
            "imported geometry preview resolves embedded materials and normal tangents");
    viewers.BeginFrame(5, false, 0);
    renderer.EndFrame();
    Require(renderer.Stats().live_textures_ == 3, "hidden model preview releases its image upload");
    runtime.Reset();
    Require(renderer.Stats().live_textures_ == 0 && renderer.Stats().live_meshes_ == 0,
            "model image GPU ownership releases on reset");
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
        Lights();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
