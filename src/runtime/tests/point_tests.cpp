#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

#include "point_ops.h"
#include "rhythm/runtime/viewers.h"

namespace {
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void Run() {
    using namespace rhythm;
    graph::Registry registry;
    graph::Document document;
    document.id_ = "points.contract";
    document.nodes_ = {
            registry.MakeNode(1, "point.emitter"), registry.MakeNode(2, "point.transform"),
            registry.MakeNode(3, "point.render"), registry.MakeNode(4, "output.texture")};
    document.edges_ = {{1, 1, 2, "points"}, {2, 2, 3, "points"}, {3, 3, 4, "source"}};
    document.output_ = 4;
    const auto plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
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
    evaluate(0);
    runtime::FrameResult result;
    for (int frame = 1; frame <= 60; ++frame) result = evaluate(frame / 60.0);
    Require(result.outputs_[0].points_ && result.outputs_[0].points_->size() == 120 &&
                    renderer.IsValid(result.final_),
            "emitter to points to texture full runtime");
    const auto retained = result.outputs_[0].points_;
    const auto first = retained->front();
    for (int frame = 0; frame < 10; ++frame) result = evaluate(1, false);
    Require(*result.outputs_[0].points_ == *retained, "pause freezes particle state");
    result = evaluate(1.1);
    Require(result.outputs_[0].points_->size() == 132 && retained->front() == first,
            "published point snapshots remain immutable across frames");
    runtime::Viewers viewers;
    Require(viewers.BeginFrame(1.1, true, 0), "point viewer due");
    renderer.BeginFrame();
    const std::array<graph::NodeId, 2> demand{1, 2};
    viewers.Capture(result, demand, renderer);
    renderer.EndFrame();
    Require(viewers.Outputs().size() == 2 && renderer.IsValid(viewers.Outputs()[0].texture_),
            "point outputs have inline graphical viewers");
    result = evaluate(1.1, true, 1);
    Require(result.outputs_[0].points_->empty(), "reset generation clears simulation history");
    auto grid = registry.MakeNode(1, "point.grid");
    grid.properties_["columns"] = 1.0;
    grid.properties_["rows"] = 1.0;
    grid.properties_["center_x"] = 0.25;
    std::array<runtime::NodeOutput, 2> outputs{};
    outputs[0].points_ = runtime::detail::GridPoints(grid);
    outputs[1].scalar_ = 90;
    graph::Instruction transform{registry.MakeNode(2, "point.transform"),
                                 graph::Operation::kPointTransform,
                                 {0, {}, 1, {}}};
    transform.node_.properties_["translate_x"] = 0.1;
    const auto transformed = runtime::detail::TransformPoints(transform, outputs);
    Require(std::abs(transformed->front().x_ - 0.6) < 1e-6 &&
                    std::abs(transformed->front().y_ - 0.25) < 1e-6 &&
                    outputs[0].points_->front().x_ == 0.25f,
            "typed transform keeps source immutable");
    const auto wide = runtime::detail::TransformPoints(transform, outputs, 2);
    Require(std::abs(wide->front().x_ - 0.6) < 1e-6 && std::abs(wide->front().y_) < 1e-6,
            "point rotation preserves screen-space geometry on a wide canvas");
    auto incompatible = document;
    incompatible.edges_[0].input_ = "scale";
    Require(std::holds_alternative<std::vector<graph::Diagnostic>>(
                    graph::Compile(incompatible, registry)),
            "point to scalar connection rejects");
    graph::Document large;
    large.id_ = "point-budget";
    large.nodes_ = {registry.MakeNode(1, "point.grid")};
    large.nodes_[0].properties_["columns"] = 128.0;
    large.nodes_[0].properties_["rows"] = 128.0;
    for (graph::NodeId id = 2; id <= 9; ++id) {
        large.nodes_.push_back(registry.MakeNode(id, "point.transform"));
        large.edges_.push_back({id, id - 1, id, "points"});
    }
    large.nodes_.push_back(registry.MakeNode(10, "point.render"));
    large.nodes_.push_back(registry.MakeNode(11, "output.texture"));
    large.edges_.push_back({10, 9, 10, "points"});
    large.edges_.push_back({11, 10, 11, "source"});
    large.output_ = 11;
    const auto excessive = graph::Compile(large, registry);
    Require(std::holds_alternative<std::vector<graph::Diagnostic>>(excessive) &&
                    std::get<std::vector<graph::Diagnostic>>(excessive).front().code_ ==
                            "graph.points_budget",
            "point capacity budget rejects before runtime allocation and publishing");
    std::cout << "point runtime: typed pipeline, pause/reset, immutable snapshots, transforms, "
                 "inline viewers and whole-graph budget passed\n";
}
}  // namespace
int main() {
    try {
        Run();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
