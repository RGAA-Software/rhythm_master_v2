#include <array>
#include <iostream>
#include <stdexcept>

#include "rhythm/runtime/viewers.h"
namespace {
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void Run() {
    using namespace rhythm;
    graph::Registry registry;
    graph::Document doc;
    doc.id_ = "gpu.runtime";
    doc.nodes_ = {registry.MakeNode(1, "gpu.particles"), registry.MakeNode(2, "gpu.render"),
                  registry.MakeNode(3, "output.texture")};
    doc.edges_ = {{1, 1, 2, "points"}, {2, 2, 3, "source"}};
    doc.output_ = 3;
    auto plan = std::get<graph::ExecutionPlan>(graph::Compile(doc, registry));
    auto renderer = render::Renderer::CreateNull();
    runtime::Runtime runtime;
    const auto evaluate = [&](double time, bool advance = true, std::uint64_t reset = 0) {
        renderer.BeginFrame();
        runtime::FrameContext frame{time, reset, {640, 360}};
        frame.advance_state_ = advance;
        auto result = runtime.Evaluate(plan, frame, renderer);
        renderer.EndFrame();
        return result;
    };
    auto result = evaluate(0);
    auto handle = result.outputs_[0].gpu_points_;
    Require(renderer.IsValid(handle) && !result.outputs_[0].points_,
            "GPU state is not a CPU point snapshot");
    Require(renderer.Stats().passes_ == 2 && renderer.Stats().gpu_point_bytes_ == 65536 * 64,
            "reset compute and draw");
    result = evaluate(1.0 / 30);
    Require(renderer.Stats().passes_ == 3 && result.outputs_[0].gpu_points_ == handle,
            "two fixed steps share the GPU allocation");
    evaluate(1.0 / 30, false);
    Require(renderer.Stats().passes_ == 1, "pause may redraw but never computes");
    Require(evaluate(1.0 / 30, false).evaluated_ == 0 && renderer.Stats().passes_ == 0,
            "stable pause is cached");
    result = evaluate(1.0 / 30);
    Require(renderer.Stats().passes_ == 1, "resume does not accumulate paused time");
    runtime::Viewers viewers;
    viewers.BeginFrame(0, true, 0);
    renderer.BeginFrame();
    const std::array<graph::NodeId, 1> nodes{1};
    viewers.Capture(result, nodes, renderer);
    Require(renderer.Stats().passes_ == 2 && renderer.Stats().draws_ == 2,
            "GPU viewer draws without simulation dispatch");
    renderer.EndFrame();
    Require(viewers.Outputs().size() == 1, "GPU state has an inline preview");
    evaluate(0);
    Require(renderer.Stats().passes_ == 2, "backward seek resets without replaying history");
    evaluate(60);
    Require(renderer.Stats().passes_ == 2, "large forward seek is bounded reset");
    result = evaluate(60, true, 1);
    Require(!renderer.IsValid(handle) && renderer.IsValid(result.outputs_[0].gpu_points_),
            "reset generation retires old observers");
    runtime.Reset();
    Require(renderer.Stats().gpu_point_bytes_ == 0, "GPU state released on runtime reset");
    // CPU point physics rejects GPU ports at graph admission, never implicit readback.
    doc.nodes_[1] = registry.MakeNode(2, "point.physics2d");
    Require(std::holds_alternative<std::vector<graph::Diagnostic>>(graph::Compile(doc, registry)),
            "typed GPU boundary");
}
}  // namespace
int main() {
    try {
        Run();
        std::cout << "GPU particle runtime: clock, ownership, previews passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what();
        return 1;
    }
}
