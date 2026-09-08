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
    document.id_ = "environment.runtime";
    document.nodes_ = {
            registry.MakeNode(1, "core.time"),       registry.MakeNode(2, "texture.gradient"),
            registry.MakeNode(3, "geometry.sphere"), registry.MakeNode(4, "material.pbr"),
            registry.MakeNode(5, "scene.instance"),  registry.MakeNode(6, "scene.environment"),
            registry.MakeNode(7, "scene.render"),    registry.MakeNode(8, "output.texture")};
    document.edges_ = {{1, 3, 5, "geometry"},
                       {2, 4, 5, "material"},
                       {3, 5, 6, "scene"},
                       {4, 2, 6, "environment_texture"},
                       {5, 1, 6, "environment_rotation"},
                       {6, 6, 7, "scene"},
                       {7, 7, 8, "source"}};
    document.output_ = 8;
    auto renderer = render::Renderer::CreateNull();
    runtime::Runtime runtime;
    runtime::Viewers viewers;
    const auto evaluate = [&](double time) {
        const auto compiled = graph::Compile(document, registry);
        Require(std::holds_alternative<graph::ExecutionPlan>(compiled),
                "environment graph compiles");
        runtime::FrameContext frame{time, 0, {128, 64}};
        frame.retained_textures_ = std::vector<graph::NodeId>{};
        renderer.BeginFrame();
        const auto result =
                runtime.Evaluate(std::get<graph::ExecutionPlan>(compiled), frame, renderer);
        renderer.EndFrame();
        return result;
    };
    const auto output = [](const runtime::FrameResult& result,
                           graph::NodeId id) -> const runtime::NodeOutput& {
        const auto found = std::find_if(result.outputs_.begin(), result.outputs_.end(),
                                        [id](const auto& value) { return value.node_ == id; });
        if (found == result.outputs_.end()) throw std::runtime_error("missing environment output");
        return *found;
    };
    (void)evaluate(0);
    Require(renderer.Stats().draws_ == 3 && renderer.Stats().live_textures_ == 4,
            "source, atlas, receiver and shared white texel have bounded ownership");
    const auto bytes = renderer.Stats().texture_bytes_;
    auto result = evaluate(1);
    Require(output(result, 6).scene_->environment_->rotation_ == 1 && renderer.Stats().draws_ == 1,
            "rotation animates receiver without reconvolving a static source");
    document.edges_.push_back({8, 1, 2, "amount"});
    result = evaluate(2);
    const auto retained = output(result, 2).texture_;
    const auto version = output(result, 2).version_;
    result = evaluate(3);
    Require(renderer.Stats().draws_ == 3 && output(result, 2).texture_ == retained &&
                    output(result, 2).version_ > version &&
                    renderer.Stats().texture_bytes_ == bytes,
            "dynamic source version refilters the same atlas and remains pinned");
    renderer.BeginFrame();
    viewers.BeginFrame(3, true, 0);
    const std::array<graph::NodeId, 1> demand{6};
    viewers.Capture(result, demand, renderer);
    Require(viewers.Outputs().size() == 1 && renderer.Stats().live_textures_ == 7,
            "inline scene viewer includes its own bounded environment atlas");
    viewers.BeginFrame(4, false, 0);
    renderer.EndFrame();
    Require(renderer.Stats().live_textures_ == 4, "hidden previews release their environment");
    auto ambiguous = document;
    ambiguous.nodes_.push_back(registry.MakeNode(9, "scene.merge"));
    ambiguous.edges_[5].from_ = 9;
    ambiguous.edges_.push_back({9, 6, 9, "a"});
    ambiguous.edges_.push_back({10, 6, 9, "b"});
    Require(std::holds_alternative<std::vector<graph::Diagnostic>>(
                    graph::Compile(ambiguous, registry)),
            "two environments reject at compile time; attach a single environment after merging");
    document.nodes_[5].properties_["environment_enabled"] = 0.0;
    (void)evaluate(4);
    Require(renderer.Stats().live_textures_ == 3, "disabled environment releases its atlas");
    runtime.Reset();
    Require(renderer.Stats().live_textures_ == 0 && renderer.Stats().live_meshes_ == 0,
            "environment reset releases every owned resource");
}
}  // namespace
int main() {
    try {
        Run();
        std::cout << "Environment runtime: cache, dynamic versions, pinning, preview, ambiguity "
                     "and release passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
