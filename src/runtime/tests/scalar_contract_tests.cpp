#include <cmath>
#include <iostream>
#include <stdexcept>

#include "rhythm/runtime/runtime.h"

namespace {
void Check(bool value) {
    if (!value) throw std::runtime_error("scalar.contract");
}
}  // namespace
int main() {
    using namespace rhythm;
    try {
        graph::Registry registry;
        graph::Document document;
        document.id_ = "scalar.test";
        document.output_ = 6;
        document.nodes_ = {
                registry.MakeNode(1, "core.time"),        registry.MakeNode(2, "time.local"),
                registry.MakeNode(3, "scalar.constant"),  registry.MakeNode(4, "scalar.math"),
                registry.MakeNode(5, "texture.gradient"), registry.MakeNode(6, "output.texture")};
        document.edges_ = {{1, 1, 2, "time"},
                           {2, 2, 4, "a"},
                           {3, 3, 4, "b"},
                           {4, 4, 5, "amount"},
                           {5, 5, 6, "source"}};
        document.nodes_[1].properties_["time_mode"] = 1.0;
        document.nodes_[1].properties_["speed"] = 2.0;
        document.nodes_[1].properties_["offset"] = -1.0;
        document.nodes_[2].properties_["value"] = 2.0;
        document.nodes_[3].properties_["math_mode"] = 2.0;
        auto renderer = render::Renderer::CreateNull();
        runtime::Runtime runtime;
        const auto evaluate = [&](double seconds) {
            const auto result = graph::Compile(document, registry);
            Check(std::holds_alternative<graph::ExecutionPlan>(result));
            renderer.BeginFrame();
            const auto frame =
                    runtime.Evaluate(std::get<graph::ExecutionPlan>(result), {seconds}, renderer);
            renderer.EndFrame();
            return frame;
        };
        const auto value = [](const runtime::FrameResult& frame, graph::NodeId id) {
            for (const auto& output : frame.outputs_)
                if (output.node_ == id) return output.scalar_;
            throw std::runtime_error("scalar.missing_output");
        };
        Check(value(evaluate(0), 2) == 3);
        Check(value(evaluate(1), 4) == 2);
        Check(value(evaluate(2.5), 4) == 0);
        const auto unchanged = evaluate(2.5);
        Check(unchanged.evaluated_ == 0 && renderer.Stats().passes_ == 0);
        document.nodes_[1].properties_["time_mode"] = 2.0;
        Check(value(evaluate(3), 2) == 3);
        document.nodes_[3].properties_["math_mode"] = 3.0;
        document.nodes_[2].properties_["value"] = 0.0;
        Check(value(evaluate(3), 4) == 0);
        document.nodes_[1].properties_["speed"] = -1.0;
        Check(std::isfinite(value(evaluate(1.0e300), 2)));
        document.nodes_[3].properties_["math_mode"] = 0.5;
        Check(std::holds_alternative<std::vector<graph::Diagnostic>>(
                graph::Compile(document, registry)));
        document.nodes_[3].properties_["math_mode"] = 0.0;
        document.nodes_[1].properties_["speed"] = 1.0;
        document.nodes_[1].properties_["offset"] = 0.0;
        document.nodes_[1].properties_["time_mode"] = 0.0;
        document.nodes_.push_back(registry.MakeNode(7, "scalar.curve"));
        document.edges_[1].from_ = 7;
        document.edges_.push_back({6, 2, 7, "time"});
        Check(value(evaluate(0.5), 4) == 0.5);
        Check(value(evaluate(3), 4) == 1);
        document.nodes_.push_back(registry.MakeNode(8, "scalar.expression"));
        document.nodes_.back().properties_["a"] = 2.0;
        document.nodes_.back().properties_["expression"] =
                parameters::Expression("a + sin(time * tau)");
        document.edges_[3].from_ = 8;
        document.edges_.push_back({7, 1, 8, "time"});
        Check(std::abs(value(evaluate(0.25), 8) - 3) < 1e-12);
        Check(evaluate(0.25).evaluated_ == 0);
        document.edges_.pop_back();
        Check(value(evaluate(0.25), 8) == 2);
        document.edges_.push_back({7, 3, 8, "a"});
        document.nodes_[2].properties_["value"] = 7.0;
        Check(value(evaluate(0.25), 8) == 7);
        document.nodes_.push_back(registry.MakeNode(9, "time.envelope"));
        auto& envelope = document.nodes_.back();
        envelope.properties_["clip_start"] = 2.0;
        envelope.properties_["clip_duration"] = 4.0;
        envelope.properties_["fade_in"] = 1.0;
        envelope.properties_["fade_out"] = 1.0;
        envelope.properties_["fade_shape"] = 0.0;
        document.edges_[3].from_ = 9;
        document.edges_.push_back({8, 1, 9, "time"});
        for (const auto& [seconds, expected] :
             {std::pair{0.0, 0.0}, {2.0, 0.0}, {2.5, 0.5}, {3.0, 1.0}, {5.5, 0.5}, {6.0, 0.0}})
            Check(value(evaluate(seconds), 9) == expected);
        Check(evaluate(6).evaluated_ == 0);
        envelope.properties_["fade_shape"] = 1.0;
        Check(value(evaluate(2.25), 9) == 0.15625);
        envelope.properties_["fade_in"] = 0.0;
        envelope.properties_["fade_out"] = 0.0;
        Check(value(evaluate(2), 9) == 1 && value(evaluate(6), 9) == 0);
        envelope.properties_["fade_in"] = 3.0;
        envelope.properties_["fade_out"] = 3.0;
        Check(value(evaluate(4), 9) == 1);
        document.edges_.back().from_ = 3;
        document.nodes_[2].properties_["value"] = 4.0;
        Check(value(evaluate(100), 9) == 1);
        Check(evaluate(101).outputs_.back().node_ == document.output_);
        // A constant local time keeps its envelope cached as global time advances.
        Check(evaluate(102).evaluated_ <= 1);
        document.id_ = "motion.test";
        document.output_ = 4;
        document.nodes_ = {
                registry.MakeNode(1, "scalar.constant"), registry.MakeNode(2, "time.phase"),
                registry.MakeNode(3, "texture.gradient"), registry.MakeNode(4, "output.texture")};
        document.edges_ = {{1, 1, 2, "speed"}, {2, 2, 3, "amount"}, {3, 3, 4, "source"}};
        runtime::FrameContext motion_frame;
        motion_frame.motion_ = runtime::MotionTime{0, 1};
        const auto sample_phase = [&] {
            const auto compiled = graph::Compile(document, registry);
            Check(std::holds_alternative<graph::ExecutionPlan>(compiled));
            renderer.BeginFrame();
            const auto result = runtime.Evaluate(std::get<graph::ExecutionPlan>(compiled),
                                                 motion_frame, renderer);
            renderer.EndFrame();
            return value(result, 2);
        };
        Check(sample_phase() == 0);
        motion_frame.seconds_ = 5;
        motion_frame.motion_->seconds_ = 5;
        Check(sample_phase() == 5);
        document.nodes_[0].properties_["value"] = 2.0;
        Check(sample_phase() == 5);  // Same-time edit changes only future motion.
        motion_frame.motion_->seconds_ = 6;
        motion_frame.seconds_ = 6;
        Check(sample_phase() == 7);
        motion_frame.extent_ = {320, 180};
        ++motion_frame.reset_generation_;  // Graphics/feedback reset, not motion seek.
        Check(sample_phase() == 7);
        motion_frame.seconds_ = 0;
        motion_frame.motion_->seconds_ = 16;
        ++motion_frame.reset_generation_;  // Natural audio loop.
        Check(sample_phase() == 11);
        motion_frame.advance_state_ = false;
        document.nodes_[0].properties_["value"] = 0.5;
        Check(sample_phase() == 11);
        motion_frame.advance_state_ = true;
        motion_frame.motion_->seconds_ = 18;
        Check(sample_phase() == 12);
        motion_frame.motion_ = runtime::MotionTime{8, 2};  // Explicit forward seek.
        Check(sample_phase() == 4);
        std::cout << "scalar/time/curve/expression/motion contracts passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
