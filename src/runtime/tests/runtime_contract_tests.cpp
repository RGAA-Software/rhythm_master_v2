#include <iostream>
#include <stdexcept>

#include "rhythm/runtime/frame_clock.h"
#include "rhythm/runtime/runtime.h"
#include "rhythm/runtime/viewers.h"

namespace {
void CheckViewerCapacity() {
    using namespace rhythm;
    auto renderer = render::Renderer::CreateNull();
    auto source = renderer.CreateTexture({16, 16});
    runtime::Viewers viewers;
    runtime::FrameResult frame;
    std::vector<graph::NodeId> nodes;
    for (std::size_t index = 0; index < runtime::Viewers::kMaxPreviews; ++index) {
        nodes.push_back(index + 1);
        frame.outputs_.push_back({index + 1, 0, source.Handle(), 1});
    }
    renderer.BeginFrame();
    viewers.BeginFrame(0, true, 0);
    viewers.Capture(frame, nodes, renderer);
    if (viewers.Outputs().size() != runtime::Viewers::kMaxPreviews)
        throw std::runtime_error("viewer.capacity");
    const auto retained = viewers.Outputs().front().texture_;
    nodes.push_back(99);
    bool rejected = false;
    try {
        viewers.Capture(frame, nodes, renderer);
    } catch (const std::length_error&) {
        rejected = true;
    }
    if (!rejected) throw std::runtime_error("viewer.unbounded");
    viewers.BeginFrame(1, false, 0);
    if (!viewers.Outputs().empty() || renderer.IsValid(retained))
        throw std::runtime_error("viewer.hidden_retained_texture");
    renderer.EndFrame();
}

void CheckViewerBudget() {
    using namespace rhythm;
    graph::Registry registry;
    graph::Document document;
    document.id_ = "viewer.test";
    document.output_ = 2;
    document.nodes_ = {
            registry.MakeNode(1, "texture.gradient"), registry.MakeNode(2, "output.texture"),
            registry.MakeNode(3, "core.time"),        registry.MakeNode(4, "signal.oscillator"),
            registry.MakeNode(5, "signal.sample"),    registry.MakeNode(6, "texture.gradient")};
    document.edges_ = {
            {1, 1, 2, "source"}, {2, 3, 4, "time"}, {3, 4, 5, "signal"}, {4, 5, 6, "amount"}};
    const std::vector<graph::NodeId> selected{6};
    auto plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry, selected));
    auto renderer = render::Renderer::CreateNull();
    runtime::Runtime runtime;
    runtime::Viewers viewers;
    std::uint32_t updates = 0;
    for (int frame = 0; frame < 60; ++frame) {
        renderer.BeginFrame();
        const auto due = viewers.BeginFrame(frame / 60.0, true, 0);
        const auto output = runtime.Evaluate(plan, {frame / 60.0, 0, {640, 360}, due}, renderer);
        viewers.Capture(output, selected, renderer);
        if (due) ++updates;
        if (!due && (output.evaluated_ || renderer.Stats().passes_))
            throw std::runtime_error("viewer.exclusive_branch_ran_early");
        if (viewers.Outputs().size() != 1 || !renderer.IsValid(viewers.Outputs()[0].texture_))
            throw std::runtime_error("viewer.no_snapshot");
        renderer.EndFrame();
    }
    if (updates != 15) throw std::runtime_error("viewer.rate");
    const auto stale = viewers.Outputs()[0].texture_;
    if (viewers.BeginFrame(1, false, 0) || renderer.IsValid(stale) || !viewers.Outputs().empty())
        throw std::runtime_error("viewer.hidden_resource");
    plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
    renderer.BeginFrame();
    runtime.Evaluate(plan, {1, 0, {640, 360}, false}, renderer);
    renderer.EndFrame();
    if (renderer.Stats().live_textures_ != 2 ||
        renderer.Stats().texture_bytes_ != 640 * 360 * 4 + 4)
        throw std::runtime_error("viewer.culled_resource");
    if (!viewers.BeginFrame(1, true, 0) || !viewers.BeginFrame(0, true, 1))
        throw std::runtime_error("viewer.reset");
}
}  // namespace

int main() {
    using namespace rhythm;
    try {
        CheckViewerCapacity();
        CheckViewerBudget();
        runtime::FrameClock clock;
        if (clock.Advance(10, false) != 0 || clock.Advance(11, false) != 1 ||
            clock.Advance(12, true) != 1 || clock.Advance(99, true) != 1 ||
            clock.Advance(100, false) != 1 || clock.Advance(101, false) != 2)
            throw std::runtime_error("runtime.suspended_clock");
        clock.Seek(4);
        if (clock.Advance(101, false) != 4 || clock.Advance(102, false) != 5 ||
            clock.Advance(104, true) != 5)
            throw std::runtime_error("runtime.seek_clock");
        clock.Seek(2);
        if (clock.Advance(105, true) != 2 || clock.Advance(106, false) != 2 ||
            clock.Advance(107, false) != 3)
            throw std::runtime_error("runtime.seek_paused_clock");
        graph::Registry registry;
        graph::Document document;
        document.id_ = "runtime.test";
        document.output_ = 4;
        document.nodes_ = {
                registry.MakeNode(1, "texture.gradient"), registry.MakeNode(2, "texture.feedback"),
                registry.MakeNode(3, "texture.blend"), registry.MakeNode(4, "output.texture")};
        document.edges_ = {
                {1, 1, 3, "a"}, {2, 2, 3, "b"}, {3, 3, 2, "source"}, {4, 3, 4, "source"}};
        const auto plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
        auto renderer = render::Renderer::CreateNull();
        runtime::Runtime runtime;
        renderer.BeginFrame();
        const auto first = runtime.Evaluate(plan, {}, renderer);
        renderer.EndFrame();
        renderer.BeginFrame();
        const auto second = runtime.Evaluate(plan, {}, renderer);
        renderer.EndFrame();
        if (first.evaluated_ != 4 || second.evaluated_ != 3 || !renderer.IsValid(second.final_))
            throw std::runtime_error("runtime.dirty_or_feedback");
        runtime::FrameContext paused;
        paused.advance_state_ = false;
        renderer.BeginFrame();
        const auto frozen = runtime.Evaluate(plan, paused, renderer);
        renderer.EndFrame();
        if (frozen.evaluated_ != 0 || frozen.final_ != second.final_ ||
            renderer.Stats().passes_ != 0)
            throw std::runtime_error("runtime.paused_feedback");
        renderer.BeginFrame();
        const auto resumed = runtime.Evaluate(plan, {}, renderer);
        renderer.EndFrame();
        if (resumed.evaluated_ != 3) throw std::runtime_error("runtime.resumed_feedback");
        renderer.BeginFrame();
        runtime.Evaluate(plan, {0, 1, {320, 180}}, renderer);
        renderer.EndFrame();
        if (renderer.IsValid(second.final_)) throw std::runtime_error("runtime.reset_generation");
        runtime.Reset();
        if (renderer.Stats().live_textures_ != 0) throw std::runtime_error("runtime.resource_leak");
        std::cout << "runtime contracts passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
