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
    std::cout << "shadow draws=" << renderer.Stats().draws_
              << " textures=" << renderer.Stats().live_textures_ << '\n';
    Require(renderer.Stats().draws_ == 2 && renderer.Stats().live_textures_ == 4,
            "shadow scene uses one receiver, a shadow pair and the shared white texel, with two "
            "draws");
    const auto bytes = renderer.Stats().texture_bytes_;
    Require(evaluate().evaluated_ == 0 && renderer.Stats().texture_bytes_ == bytes,
            "unchanged shadow scene does not redraw or reallocate");
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
