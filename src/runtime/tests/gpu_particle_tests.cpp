#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>

#include "rhythm/runtime/viewers.h"
namespace {
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void Mapping() {
    using namespace rhythm;
    graph::Registry registry;
    graph::Document doc;
    doc.id_ = "gpu.mapping.runtime";
    doc.nodes_ = {registry.MakeNode(1, "gpu.particles"),  registry.MakeNode(2, "gpu.map"),
                  registry.MakeNode(3, "gpu.map"),        registry.MakeNode(4, "gpu.render"),
                  registry.MakeNode(5, "output.texture"), registry.MakeNode(6, "scalar.constant")};
    doc.nodes_[0].properties_["particle_capacity"] = 65.0;
    doc.edges_ = {{1, 1, 2, "points"},
                  {2, 2, 3, "points"},
                  {3, 3, 4, "points"},
                  {4, 4, 5, "source"},
                  {5, 6, 2, "point_size_scale"}};
    doc.output_ = 5;
    auto plan = std::get<graph::ExecutionPlan>(graph::Compile(doc, registry));
    auto renderer = render::Renderer::CreateNull();
    runtime::Runtime runtime;
    runtime::FrameContext context{0, 0, {64, 64}};
    context.advance_state_ = false;
    const auto evaluate = [&] {
        renderer.BeginFrame();
        auto frame = runtime.Evaluate(plan, context, renderer);
        renderer.EndFrame();
        return frame;
    };
    const auto find = [](const runtime::FrameResult& frame, graph::NodeId id) {
        const auto found = std::find_if(frame.outputs_.begin(), frame.outputs_.end(),
                                        [id](const auto& output) { return output.node_ == id; });
        if (found == frame.outputs_.end()) throw std::runtime_error("mapping output missing");
        return *found;
    };
    const auto initial = evaluate();
    const auto source = find(initial, 1).gpu_points_;
    const auto first = find(initial, 2).gpu_points_;
    const auto second = find(initial, 3).gpu_points_;
    Require(source != first && first != second && source != second &&
                    find(initial, 3).gpu_point_capacity_ == 65 &&
                    renderer.Stats().gpu_point_buffers_ == 3 &&
                    renderer.Stats().gpu_point_bytes_ == 65 * 64 * 3 &&
                    renderer.Stats().passes_ == 4,
            "mapping initializes distinct budgeted buffers");
    Require(evaluate().evaluated_ == 0 && renderer.Stats().passes_ == 0,
            "stable paused mapping performs no GPU work");
    doc.nodes_[5].properties_["value"] = .5;
    plan = std::get<graph::ExecutionPlan>(graph::Compile(doc, registry));
    const auto changed = evaluate();
    Require(find(changed, 1).gpu_points_ == source && find(changed, 2).gpu_points_ == first &&
                    find(changed, 3).gpu_points_ == second && renderer.Stats().passes_ == 3,
            "changed scalar remaps two stages without reallocating or simulating");
    runtime::Viewers viewers;
    viewers.BeginFrame(0, true, 0);
    renderer.BeginFrame();
    const std::array<graph::NodeId, 2> nodes{2, 3};
    viewers.Capture(changed, nodes, renderer);
    renderer.EndFrame();
    Require(viewers.Outputs().size() == 2 && renderer.Stats().passes_ == 4 &&
                    renderer.Stats().draws_ == 4,
            "derived point previews draw without map dispatch");
    viewers.BeginFrame(0, false, 0);
    doc.nodes_[0].properties_["particle_capacity"] = 129.0;
    plan = std::get<graph::ExecutionPlan>(graph::Compile(doc, registry));
    const auto resized = evaluate();
    Require(!renderer.IsValid(source) && !renderer.IsValid(first) && !renderer.IsValid(second) &&
                    find(resized, 3).gpu_point_capacity_ == 129 &&
                    renderer.Stats().gpu_point_bytes_ == 129 * 64 * 3,
            "source capacity changes rebuild all derived buffers");
    runtime.Reset();
    Require(renderer.Stats().gpu_point_buffers_ == 0, "mapping reset releases all owners");
    runtime.BeginPreparation(plan, context);
    for (int index = 0; index < 3; ++index) {
        renderer.BeginFrame();
        const auto progress = runtime.PrepareNext(renderer, {1, 100});
        renderer.EndFrame();
        Require(progress.state_ != runtime::PreparationState::kFailed,
                "staged mapping preparation must succeed");
    }
    Require(renderer.Stats().gpu_point_buffers_ >= 2, "cancel exercises an allocated map");
    runtime.Reset();
    Require(renderer.Stats().gpu_point_buffers_ == 0, "cancel releases partial mapping state");
    runtime.BeginPreparation(plan, context);
    bool ready = false;
    for (int index = 0; index < 16 && !ready; ++index) {
        renderer.BeginFrame();
        const auto progress = runtime.PrepareNext(renderer, {1, 100});
        renderer.EndFrame();
        Require(progress.state_ != runtime::PreparationState::kFailed,
                "mapping preparation failed");
        ready = progress.state_ == runtime::PreparationState::kReady;
    }
    Require(ready && renderer.Stats().gpu_point_buffers_ == 3, "staged mapping becomes ready");
    runtime.Reset();
    doc.nodes_.push_back(registry.MakeNode(7, "texture.gradient"));
    doc.edges_[3].from_ = 7;
    plan = std::get<graph::ExecutionPlan>(graph::Compile(doc, registry));
    evaluate();
    Require(renderer.Stats().gpu_point_buffers_ == 0,
            "unrequested mapping chain allocates nothing");
    std::cout << "GPU mapping runtime: ownership, cache, controls, previews, resize, staging, "
                 "cancel and demand passed (Null backend)\n";
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
        Mapping();
        std::cout << "GPU particle runtime: clock, ownership, previews passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what();
        return 1;
    }
}
