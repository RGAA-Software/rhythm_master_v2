#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "rhythm/runtime/signal_previews.h"

namespace {
void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void Run() {
    using namespace rhythm;
    graph::Registry registry;
    graph::Document document;
    document.id_ = "signal.preview";
    document.output_ = 2;
    document.nodes_ = {registry.MakeNode(1, "texture.gradient"),
                       registry.MakeNode(2, "output.texture"), registry.MakeNode(3, "core.time"),
                       registry.MakeNode(4, "signal.oscillator")};
    document.edges_ = {{1, 1, 2, "source"}, {2, 3, 4, "time"}};
    const std::vector<graph::NodeId> nodes{3, 4};
    const auto plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry, nodes));
    auto renderer = render::Renderer::CreateNull();
    runtime::Runtime runtime;
    runtime::SignalPreviews previews;
    runtime::FrameResult output;
    for (int frame = 0; frame < 200; ++frame) {
        const auto seconds = frame / 15.0;
        renderer.BeginFrame();
        output = runtime.Evaluate(plan, {seconds, 0, {32, 32}}, renderer);
        previews.Capture(output, nodes, seconds, 0);
        renderer.EndFrame();
    }
    const auto& time = previews.Traces().at(3);
    Check(time.count_ == 120 && time.offset_ == 80, "history must remain bounded and ordered");
    Check(std::abs(time.samples_[time.offset_] - 80.0f / 15) < 1e-5,
          "oldest retained sample must match actual runtime time");
    Check(std::abs(time.value_ - 199.0 / 15) < 1e-9, "displayed value must match runtime");
    Check(previews.Traces().at(4).samples_[0] != previews.Traces().at(4).samples_[1],
          "signal preview must sample the evaluated oscillator branch");
    const auto before = time;
    for (int frame = 0; frame < 200; ++frame) previews.Capture(output, nodes, 199.0 / 15, 0);
    Check(previews.Traces().at(3).samples_ == before.samples_ &&
                  previews.Traces().at(3).offset_ == before.offset_,
          "paused playback must not scroll history");
    previews.Capture(output, nodes, 1, 0);
    Check(previews.Traces().at(3).count_ == 1, "backward seek must reset history");
    previews.Capture(output, nodes, 2, 1);
    Check(previews.Traces().at(3).count_ == 1, "forward seek generation must reset history");
    previews.Capture(output, std::span(nodes).first(1), 3, 1);
    Check(previews.Traces().size() == 1, "hidden node history must be released");
    output.outputs_ = {{3, std::numeric_limits<double>::infinity()}};
    previews.Capture(output, nodes, 4, 1);
    Check(previews.Traces().empty(), "invalid and missing outputs must clear stale previews");
    output.outputs_ = {{3, std::numeric_limits<double>::max()}};
    previews.Capture(output, nodes, 5, 1);
    Check(std::isfinite(previews.Traces().at(3).samples_[0]), "plot coordinates must be finite");
    bool rejected = false;
    try {
        previews.Capture(output, std::vector<graph::NodeId>(9), 6, 1);
    } catch (const std::length_error&) {
        rejected = true;
    }
    Check(rejected, "unbounded preview demand must be rejected");
    previews.Clear();
    Check(previews.Traces().empty(), "plan invalidation must clear all history");

    document.beat_grid_ = parameters::BeatSettings{};
    document.nodes_.push_back(registry.MakeNode(5, "event.beat"));
    const std::array<graph::NodeId, 1> event_nodes{5};
    const auto event_plan =
            std::get<graph::ExecutionPlan>(graph::Compile(document, registry, event_nodes));
    runtime.Reset();
    const auto evaluate = [&](double seconds) {
        renderer.BeginFrame();
        auto result = runtime.Evaluate(event_plan, {seconds, 0, {32, 32}}, renderer);
        renderer.EndFrame();
        return result;
    };
    previews.Capture(evaluate(0), event_nodes, 0, 0);
    previews.Capture(evaluate(0.49), event_nodes, 0.49, 0);
    evaluate(0.5);  // The actual pulse frame is between preview captures.
    output = evaluate(0.51);
    previews.Capture(output, event_nodes, 0.51, 0);
    Check(previews.Traces().at(5).value_ == 1 &&
                  previews.Traces().at(5).event_observation_->count_ == 1,
          "preview missed a pulse between its samples");
    const auto event_trace = previews.Traces().at(5);
    previews.Capture(output, event_nodes, 0.51, 0);
    Check(previews.Traces().at(5).samples_ == event_trace.samples_,
          "repeated preview counted the same event twice");
    previews.Capture(evaluate(0.6), event_nodes, 0.6, 0);
    Check(previews.Traces().at(5).value_ == 0 &&
                  previews.Traces().at(5).event_observation_->count_ == 1,
          "empty event interval lost cumulative observation or retained a pulse");
    evaluate(1);
    evaluate(1.5);
    previews.Capture(evaluate(1.6), event_nodes, 1.6, 0);
    Check(previews.Traces().at(5).value_ == 2, "slow preview did not retain multiple pulses");
    output = evaluate(1000);
    Check(output.rejected_event_total_ > 0, "overload diagnostic was not retained");
    Check(evaluate(1000.01).rejected_event_total_ == output.rejected_event_total_,
          "overload diagnostic disappeared before the UI could observe it");
    runtime.Reset();
    previews.Capture(evaluate(0), event_nodes, 0, 1);
    Check(previews.Traces().at(5).event_observation_->count_ == 0 &&
                  previews.Traces().at(5).count_ == 1,
          "restart retained event observations from previous playback");
}
}  // namespace
int main() {
    try {
        Run();
        std::cout
                << "Signal previews: evaluated branches, bounded history, pause and seek passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
