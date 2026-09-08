#include <array>
#include <iostream>
#include <stdexcept>

#include "blur_pass.h"
#include "rhythm/runtime/runtime.h"
#include "texture_lifetimes.h"
namespace {
void Require(bool value, const char* text) {
    if (!value) throw std::runtime_error(text);
}
void Run() {
    using namespace rhythm;
    graph::Registry registry;
    graph::Document doc;
    doc.id_ = "color.runtime";
    doc.nodes_ = {
            registry.MakeNode(1, "core.time"),         registry.MakeNode(2, "texture.gradient"),
            registry.MakeNode(3, "texture.linearize"), registry.MakeNode(4, "texture.color_adjust"),
            registry.MakeNode(5, "texture.blur"),      registry.MakeNode(6, "texture.display"),
            registry.MakeNode(7, "output.texture")};
    doc.nodes_[3].properties_["exposure"] = 3.0;
    doc.edges_ = {{1, 1, 2, "amount"}, {2, 2, 3, "source"}, {3, 3, 4, "source"},
                  {4, 4, 5, "source"}, {5, 5, 6, "source"}, {6, 6, 7, "source"}};
    doc.output_ = 7;
    auto renderer = render::Renderer::CreateNull();
    runtime::Runtime runtime;
    const auto evaluate = [&](double seconds, bool reuse) {
        auto compiled = graph::Compile(doc, registry);
        if (!std::holds_alternative<graph::ExecutionPlan>(compiled))
            throw std::runtime_error("color.graph");
        auto plan = std::get<graph::ExecutionPlan>(std::move(compiled));
        runtime::FrameContext frame{seconds, 0, {64, 64}};
        if (reuse) frame.retained_textures_ = std::vector<graph::NodeId>{};
        renderer.BeginFrame();
        auto result = runtime.Evaluate(plan, frame, renderer);
        renderer.EndFrame();
        return result;
    };
    auto frame = evaluate(0, false);
    Require(renderer.Precision(frame.outputs_[1].texture_) == render::TexturePrecision::kUnorm8,
            "generator default is SDR");
    for (std::size_t i = 2; i <= 4; ++i)
        Require(renderer.Precision(frame.outputs_[i].texture_) ==
                        render::TexturePrecision::kFloat16,
                "linearize/color/blur preserve float precision");
    Require(renderer.Precision(frame.final_) == render::TexturePrecision::kUnorm8,
            "display mapping returns SDR");
    const auto previous = frame.outputs_[3].texture_;
    doc.nodes_[3].properties_["texture_precision"] = 1.0;
    frame = evaluate(1.0 / 60, false);
    Require(!renderer.IsValid(previous) && renderer.Precision(frame.outputs_[3].texture_) ==
                                                   render::TexturePrecision::kUnorm8,
            "precision change reallocates and retires prior target");
    doc.nodes_[3].properties_["texture_precision"] = 0.0;
    evaluate(2.0 / 60, true);
    const auto stable = renderer.Stats().texture_bytes_;
    for (int i = 3; i < 20; ++i) evaluate(i / 60.0, true);
    Require(renderer.Stats().texture_bytes_ == stable, "mixed precision reuse remains bounded");
    runtime.Reset();
    Require(renderer.Stats().texture_bytes_ == 0, "float graph releases targets");
    runtime::detail::TexturePool pool;
    pool.BeginFrame();
    auto floating = renderer.CreateTexture({16, 16}, {}, render::TexturePrecision::kFloat16);
    const auto floating_handle = floating.Handle();
    pool.Recycle(std::move(floating), {16, 16}, render::TexturePrecision::kFloat16);
    auto bytes = pool.Acquire({16, 16}, renderer, render::TexturePrecision::kUnorm8);
    Require(bytes.Handle() != floating_handle, "same extent cannot reuse a different pixel format");
    runtime::detail::BlurPass blur;
    renderer.BeginFrame();
    auto result =
            blur.Draw(bytes.Handle(), {16, 16}, 0, renderer, render::TexturePrecision::kFloat16);
    Require(result != bytes.Handle() &&
                    renderer.Precision(result) == render::TexturePrecision::kFloat16,
            "zero-radius blur still honors explicit precision conversion");
    renderer.EndFrame();
    doc.nodes_[3].properties_.erase("texture_precision");
    frame = evaluate(0, false);
    Require(renderer.Precision(frame.outputs_[3].texture_) == render::TexturePrecision::kUnorm8,
            "legacy property omission preserves SDR clipping behavior");
}
}  // namespace
int main() {
    try {
        Run();
        std::cout << "Float graph: precision/legacy/cache/blur passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what();
        return 1;
    }
}
