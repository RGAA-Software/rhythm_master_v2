#include <algorithm>
#include <iostream>
#include <stdexcept>

#include "rhythm/runtime/viewers.h"
#include "scene_pass.h"

namespace {
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void Run() {
    using namespace rhythm;
    graph::Registry registry;
    graph::Document document;
    document.id_ = "material.runtime";
    document.nodes_ = {
            registry.MakeNode(1, "core.time"),       registry.MakeNode(2, "texture.gradient"),
            registry.MakeNode(3, "material.pbr"),    registry.MakeNode(4, "material.textures"),
            registry.MakeNode(5, "geometry.sphere"), registry.MakeNode(6, "scene.instance"),
            registry.MakeNode(7, "scene.capture"),   registry.MakeNode(8, "scene.color"),
            registry.MakeNode(9, "output.texture")};
    document.edges_ = {
            {1, 1, 2, "amount"},         {2, 3, 4, "material"}, {3, 2, 4, "base_texture"},
            {4, 2, 4, "normal_texture"}, {5, 5, 6, "geometry"}, {6, 4, 6, "material"},
            {7, 6, 7, "scene"},          {8, 7, 8, "capture"},  {9, 8, 9, "source"}};
    document.output_ = 9;
    document.nodes_[3].properties_["uv_scale_x"] = 2.0;
    auto renderer = render::Renderer::CreateNull();
    runtime::Runtime runtime;
    runtime::Viewers viewers;
    runtime::detail::ScenePass pass;
    const auto plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
    render::TextureHandle retained;
    std::uint64_t previous = 0;
    for (int i = 0; i < 3; ++i) {
        runtime::FrameContext context{double(i), 0, {128, 64}};
        context.retained_textures_ = std::vector<graph::NodeId>{};
        renderer.BeginFrame();
        const auto result = runtime.Evaluate(plan, context, renderer);
        const auto output = [&](graph::NodeId id) -> const runtime::NodeOutput& {
            const auto found = std::find_if(result.outputs_.begin(), result.outputs_.end(),
                                            [id](const auto& value) { return value.node_ == id; });
            if (found == result.outputs_.end()) throw std::runtime_error("missing output");
            return *found;
        };
        const auto texture = output(2).texture_;
        Require(renderer.IsValid(texture) && output(2).version_ > previous,
                "dynamic material source stays alive after its direct consumer has evaluated");
        if (i) Require(texture == retained, "material input has a stable persistent owner");
        retained = texture;
        previous = output(2).version_;
        const auto list = pass.Build(*output(6).scene_, {}, {128, 64}, renderer, result.outputs_);
        Require(list.draws_[0].textures_.slots_[0] == texture &&
                        list.draws_[0].textures_.slots_[1] == texture &&
                        list.draws_[0].textures_.uv_transform_[0] == 2,
                "scene material resolves producer IDs to the current frame texture");
        viewers.BeginFrame(double(i), true, 0);
        const std::array<graph::NodeId, 2> demand{4, 6};
        viewers.Capture(result, demand, renderer);
        Require(viewers.Outputs().size() == 2, "material and scene previews resolve texture slots");
        renderer.EndFrame();
    }
    viewers.BeginFrame(4, false, 0);
    pass = {};
    runtime.Reset();
    Require(renderer.Stats().live_textures_ == 0 && renderer.Stats().live_meshes_ == 0,
            "material source, tangent uploads and captured targets release together");
}
}  // namespace
int main() {
    try {
        Run();
        std::cout
                << "Material graph lifetime, tangent upload, dynamic capture and previews passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
