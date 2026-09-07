#pragma once

#include "rhythm/project/package.h"
#include "rhythm/runtime/viewers.h"

namespace rhythm::validation {
// Shared Windows/Android real-GPU acceptance through graph compilation and the
// published program codec. Scene resources stay in the ordinary runtime path.
class SceneGraphFixture final {
   public:
    void Draw(render::Renderer& renderer, int scenario) {
        if (scenario != scenario_) {
            graph::Registry registry;
            graph::Document document;
            document.id_ = "probe.scene_graph";
            document.canvas_ = {16, 16};
            document.nodes_ = {
                    registry.MakeNode(1, scenario == 0 ? "geometry.cube" : "geometry.sphere"),
                    registry.MakeNode(2, "material.unlit"),
                    registry.MakeNode(3, "scene.instance"),
                    registry.MakeNode(4, "scene.transform"),
                    registry.MakeNode(5, "scene.camera"),
                    registry.MakeNode(6, "scene.render"),
                    registry.MakeNode(7, "output.texture")};
            document.nodes_[1].properties_["color_a"] =
                    scenario == 3 ? graph::Color{0, 0, 1, 1} : graph::Color{0, 1, 0, 1};
            document.nodes_[3].properties_["translate_x"] = scenario == 2 ? 3.0 : 0.0;
            document.nodes_[4].properties_["projection"] = scenario == 4 ? 1.0 : 0.0;
            document.nodes_[4].properties_["near_plane"] = scenario == 5 ? 4.0 : 0.05;
            document.edges_ = {{1, 1, 3, "geometry"}, {2, 2, 3, "material"}, {3, 3, 4, "scene"},
                               {4, 4, 6, "scene"},    {5, 5, 6, "camera"},   {6, 6, 7, "source"}};
            document.output_ = 7;
            if (scenario >= 7) {
                document.nodes_[0] = registry.MakeNode(1, "geometry.cube");
                document.nodes_[1] = registry.MakeNode(2, "material.pbr");
                document.nodes_[1].properties_["color_a"] = graph::Color{1, 1, 1, 1};
                document.nodes_[1].properties_["roughness"] = 1.0;
                if (scenario == 8) document.nodes_[1].properties_["metallic"] = 1.0;
                document.nodes_.push_back(registry.MakeNode(8, "scene.directional_light"));
                document.nodes_.back().properties_["light_x"] = 0.0;
                document.nodes_.back().properties_["light_y"] = 0.0;
                document.nodes_.back().properties_["light_energy"] = 1.0;
                document.nodes_.push_back(registry.MakeNode(9, "scene.merge"));
                document.edges_[3].from_ = 9;
                document.edges_.push_back({7, 4, 9, "a"});
                document.edges_.push_back({8, 8, 9, "b"});
            }
            const auto plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
            plan_ = project::DecodeProgram(project::EncodeProgram(plan));
            scenario_ = scenario;
        }
        renderer.BeginFrame();
        const auto result = runtime_.Evaluate(plan_, {0, 0, {16, 16}}, renderer);
        auto texture = result.final_;
        if (scenario == 6) {
            viewers_.BeginFrame(seconds_, true, 0);
            const std::array<graph::NodeId, 1> nodes{4};
            viewers_.Capture(result, nodes, renderer);
            texture = viewers_.Outputs().front().texture_;
        }
        seconds_ += 0.1;
        render::DrawList draw;
        draw.width_ = draw.height_ = 16;
        draw.vertices_ = {{0, 0, 0, 0}, {16, 0, 1, 0}, {16, 16, 1, 1}, {0, 16, 0, 1}};
        draw.indices_ = {0, 1, 2, 0, 2, 3};
        draw.commands_ = {{texture, 0, 6, {0, 0, 16, 16}}};
        renderer.Submit({}, draw);
        renderer.EndFrame();
    }

   private:
    int scenario_ = -1;
    double seconds_ = 0;
    graph::ExecutionPlan plan_{};
    runtime::Runtime runtime_{};
    runtime::Viewers viewers_{};
};
}  // namespace rhythm::validation
