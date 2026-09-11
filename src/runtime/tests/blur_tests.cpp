#include <iostream>
#include <stdexcept>

#include "blur_pass.h"
#include "rhythm/graph/compiler.h"
#include "rhythm/runtime/runtime.h"

namespace {
void Check(bool value) {
    if (!value) throw std::runtime_error("blur.contract");
}
}  // namespace
int main() {
    using namespace rhythm;
    try {
        auto renderer = render::Renderer::CreateNull();
        auto source = renderer.CreateTexture({128, 128});
        {
            runtime::detail::BlurPass blur;
            renderer.BeginFrame();
            Check(blur.Draw(source.Handle(), {128, 128}, 0, renderer) == source.Handle());
            Check(renderer.Stats().passes_ == 0);
            const auto result = blur.Draw(source.Handle(), {128, 128}, 16, renderer);
            Check(result != source.Handle() && renderer.IsValid(result));
            Check(renderer.Stats().passes_ == 5);
            const auto textures = renderer.Stats().live_textures_;
            Check(blur.Draw(source.Handle(), {128, 128}, 16, renderer) == result);
            Check(renderer.Stats().live_textures_ == textures);
            blur.Draw(source.Handle(), {128, 128}, 1, renderer);
            Check(renderer.Stats().live_textures_ == 2);
            Check(!renderer.IsValid(result));
            renderer.EndFrame();
        }
        Check(renderer.Stats().live_textures_ == 1);
        graph::Registry registry;
        graph::Document document;
        document.id_ = "blur.runtime";
        document.nodes_ = {registry.MakeNode(1, "texture.shape"),
                           registry.MakeNode(2, "texture.blur"),
                           registry.MakeNode(3, "output.texture")};
        document.edges_ = {{1, 1, 2, "source"}, {2, 2, 3, "source"}};
        document.output_ = 3;
        auto compiled = graph::Compile(document, registry);
        Check(std::holds_alternative<graph::ExecutionPlan>(compiled));
        runtime::Runtime runtime;
        renderer.BeginFrame();
        const auto frame = runtime.Evaluate(std::get<graph::ExecutionPlan>(compiled),
                                            {.extent_ = {128, 128}}, renderer);
        Check(renderer.IsValid(frame.final_));
        renderer.EndFrame();
        renderer.BeginFrame();
        const auto cached = runtime.Evaluate(std::get<graph::ExecutionPlan>(compiled),
                                             {.extent_ = {128, 128}}, renderer);
        Check(cached.evaluated_ == 0 && cached.final_ == frame.final_);
        renderer.EndFrame();
        runtime.Reset();
        Check(renderer.Stats().live_textures_ == 1);
        std::cout << "Blur: zero bypass, bounded pyramid, reuse, release and graph cache passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
