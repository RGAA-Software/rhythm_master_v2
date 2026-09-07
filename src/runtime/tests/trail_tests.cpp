#include <iostream>
#include <stdexcept>

#include "rhythm/graph/compiler.h"
#include "rhythm/runtime/runtime.h"
#include "trail_pass.h"

namespace {
void Check(bool value) {
    if (!value) throw std::runtime_error("trail.contract");
}
}  // namespace
int main() {
    using namespace rhythm;
    try {
        auto renderer = render::Renderer::CreateNull();
        auto source = renderer.CreateTexture({16, 16});
        {
            runtime::detail::TrailPass trail;
            renderer.BeginFrame();
            const auto first = trail.Draw(source.Handle(), {16, 16}, 0, true, {}, renderer);
            Check(renderer.Stats().texture_bytes_ == 16 * 16 * 20);
            Check(renderer.Stats().passes_ == 1);
            Check(trail.Draw(source.Handle(), {16, 16}, 0, true, {}, renderer) == first);
            Check(trail.Draw(source.Handle(), {16, 16}, 5, false, {}, renderer) == first);
            Check(renderer.Stats().passes_ == 1);
            const auto next = trail.Draw(source.Handle(), {16, 16}, 5.01, true, {}, renderer);
            Check(next != first && renderer.Stats().passes_ == 2);
            trail.Draw(source.Handle(), {16, 16}, 1, true, {}, renderer);
            Check(renderer.Stats().live_textures_ == 3);
            trail.Draw(source.Handle(), {8, 8}, 1.01, true, {}, renderer);
            Check(!renderer.IsValid(first) && !renderer.IsValid(next));
            Check(trail.Draw(source.Handle(), {8, 8}, 2, true, {0}, renderer) == source.Handle());
            Check(renderer.Stats().live_textures_ == 1);
            renderer.EndFrame();
        }
        graph::Registry registry;
        graph::Document document;
        document.id_ = "trail.runtime";
        document.nodes_ = {registry.MakeNode(1, "texture.shape"),
                           registry.MakeNode(2, "texture.trail"),
                           registry.MakeNode(3, "output.texture")};
        document.edges_ = {{1, 1, 2, "source"}, {2, 2, 3, "source"}};
        document.output_ = 3;
        const auto plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
        runtime::Runtime runtime;
        const auto evaluate = [&](double time, bool advance, std::uint64_t reset) {
            renderer.BeginFrame();
            const auto result = runtime.Evaluate(plan,
                                                 {.seconds_ = time,
                                                  .reset_generation_ = reset,
                                                  .extent_ = {16, 16},
                                                  .advance_state_ = advance},
                                                 renderer);
            renderer.EndFrame();
            return result;
        };
        const auto first = evaluate(0, true, 0);
        Check(evaluate(0, true, 0).evaluated_ == 0);
        Check(evaluate(0.01, true, 0).final_ != first.final_);
        const auto paused = evaluate(0.02, false, 0);
        Check(renderer.Stats().passes_ == 0);
        Check(evaluate(0.03, false, 0).final_ == paused.final_);
        Check(evaluate(0.04, true, 0).final_ != paused.final_);
        evaluate(0, true, 1);
        Check(!renderer.IsValid(first.final_));
        runtime.Reset();
        Check(renderer.Stats().live_textures_ == 1);
        std::cout << "Trail: pause, repeat, resize, bypass, reset, cache and bounded resources "
                     "passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
