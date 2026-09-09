#include <cmath>
#include <iostream>
#include <source_location>
#include <stdexcept>
#include <string>

#include "rhythm/runtime/runtime.h"

namespace {
void Check(bool value, std::source_location location = std::source_location::current()) {
    if (!value) throw std::runtime_error("event.runtime:" + std::to_string(location.line()));
}
rhythm::runtime::NodeOutput Output(const rhythm::runtime::FrameResult& frame,
                                   rhythm::graph::NodeId id) {
    for (const auto& output : frame.outputs_)
        if (output.node_ == id) return output;
    throw std::runtime_error("event output missing");
}
}  // namespace
int main() {
    using namespace rhythm;
    try {
        graph::Registry registry;
        graph::Document document;
        document.id_ = "events-runtime";
        document.beat_grid_ = parameters::BeatSettings{};
        document.nodes_ = {
                registry.MakeNode(1, "event.beat"),     registry.MakeNode(2, "event.merge"),
                registry.MakeNode(3, "event.step"),     registry.MakeNode(4, "texture.gradient"),
                registry.MakeNode(5, "output.texture"), registry.MakeNode(6, "event.envelope")};
        document.edges_ = {{1, 1, 2, "a"},      {2, 1, 2, "b"},      {3, 2, 3, "events"},
                           {4, 3, 4, "amount"}, {5, 4, 5, "source"}, {6, 2, 6, "events"}};
        document.output_ = 5;
        auto renderer = render::Renderer::CreateNull();
        runtime::Runtime runtime;
        runtime::FrameContext context;
        context.retained_textures_ = std::vector<graph::NodeId>{};
        const auto evaluate = [&](double seconds) {
            context.seconds_ = seconds;
            const auto compiled =
                    graph::Compile(document, registry, std::array<graph::NodeId, 1>{6});
            Check(std::holds_alternative<graph::ExecutionPlan>(compiled));
            renderer.BeginFrame();
            auto output =
                    runtime.Evaluate(std::get<graph::ExecutionPlan>(compiled), context, renderer);
            renderer.EndFrame();
            Check(renderer.IsValid(output.final_));
            return output;
        };
        Check(Output(evaluate(0), 3).scalar_ == 0);
        Check(Output(evaluate(0.49), 3).scalar_ == 0);
        context.advance_state_ = false;
        Check(Output(evaluate(0.49), 3).scalar_ == 0);
        context.advance_state_ = true;
        auto result = evaluate(0.5);
        Check(!result.rejected_events_ && Output(result, 3).scalar_ == 1);
        Check(Output(result, 1).events_->Events().size() == 1);
        Check(Output(result, 2).events_->Events().size() == 1);  // Duplicate branch merged once.
        const auto first_sequence = Output(result, 1).events_->Events()[0].sequence_;
        runtime.Reset();
        evaluate(0);
        result = evaluate(0.5);
        Check(Output(result, 1).events_->Events()[0].sequence_ > first_sequence);
        Check(Output(evaluate(0.5), 3).scalar_ == 1);
        Check(std::abs(Output(evaluate(0.505), 6).scalar_ - 0.5) < 1e-9);
        Check(!Output(evaluate(0.51), 1).events_);
        Check(Output(evaluate(1), 3).scalar_ == 2);
        document.nodes_[0].properties_["beat_interval"] = 2.0;
        Check(Output(evaluate(1.25), 3).scalar_ == 2);
        Check(Output(evaluate(2), 3).scalar_ == 3);  // Source edit must not reuse consumed IDs.
        ++context.reset_generation_;
        Check(Output(evaluate(0), 3).scalar_ == 0);
        Check(Output(evaluate(1), 3).scalar_ == 1);

        // Actual audio feature snapshots, including producer restarts and repeat
        // observations, feed one event at the first observing display frame.
        document.nodes_[0] = registry.MakeNode(1, "event.audio_onset");
        ++context.reset_generation_;
        audio::Features audio;
        audio.generation_ = 4;
        audio.onset_id_ = 1;
        audio.sample_rate_ = 48000;
        audio.valid_ = true;
        audio.mono_bands_[10] = 0.6f;
        context.external_.audio_ = audio;
        Check(Output(evaluate(0), 3).scalar_ == 0);
        ++context.external_.audio_->onset_id_;
        result = evaluate(0.1);
        Check(Output(result, 3).scalar_ == 1 &&
              Output(result, 1).events_->Events()[0].seconds_ == 0.1);
        Check(Output(evaluate(0.2), 3).scalar_ == 1);
        ++context.external_.audio_->onset_id_;
        context.external_.audio_->mono_bands_[10] = 0;
        Check(Output(evaluate(0.3), 3).scalar_ == 1);
        ++context.external_.audio_->generation_;
        context.external_.audio_->onset_id_ = 1;
        context.external_.audio_->mono_bands_[10] = 0.6f;
        Check(Output(evaluate(0.4), 3).scalar_ == 1);
        ++context.external_.audio_->onset_id_;
        Check(Output(evaluate(0.5), 3).scalar_ == 2);

        // Seek/load resets state; a very large unmarked jump remains bounded and
        // reports rejected events rather than recursing or silently claiming all.
        document.nodes_[0] = registry.MakeNode(1, "event.beat");
        ++context.reset_generation_;
        evaluate(0);
        result = evaluate(1000);
        Check(result.rejected_events_ > 0 && Output(result, 1).events_->Events().size() == 256);
        ++context.reset_generation_;
        result = evaluate(1000);
        Check(!result.rejected_events_ && !Output(result, 1).events_ &&
              Output(result, 3).scalar_ == 0);
        document.nodes_[0] = registry.MakeNode(1, "event.edge");
        document.nodes_[0].properties_["edge_mode"] = 1.0;
        document.nodes_[2] = registry.MakeNode(3, "event.gate");
        document.nodes_.push_back(registry.MakeNode(7, "scalar.constant"));
        document.nodes_.back().properties_["value"] = 0.0;
        document.edges_.push_back({7, 7, 1, "value"});
        document.edges_.push_back({8, 7, 3, "value"});
        ++context.reset_generation_;
        Check(Output(evaluate(0), 3).scalar_ == 0);
        document.nodes_.back().properties_["value"] = 0.8;
        Check(Output(evaluate(0.1), 3).scalar_ == 0.8);
        document.nodes_.back().properties_["value"] = 0.7;
        Check(Output(evaluate(0.2), 3).scalar_ == 0.7);
        document.nodes_.back().properties_["value"] = 0.48;
        Check(Output(evaluate(0.3), 3).scalar_ == 0);
        document.nodes_[2] = registry.MakeNode(3, "event.latch");
        evaluate(0.4);
        document.nodes_.back().properties_["value"] = 0.9;
        Check(Output(evaluate(0.5), 3).scalar_ == 0.9);
        document.nodes_.back().properties_["value"] = 0.8;
        Check(Output(evaluate(0.6), 3).scalar_ == 0.9);
        document.nodes_.back().properties_["value"] = 0.2;
        Check(Output(evaluate(0.7), 3).scalar_ == 0.9);
        document.nodes_[1] = registry.MakeNode(2, "event.reset");
        document.edges_[0].input_ = "events";
        document.edges_.erase(document.edges_.begin() + 1);
        evaluate(0.8);
        document.nodes_.back().properties_["value"] = 0.9;
        result = evaluate(0.9);
        Check(Output(result, 2).events_->Events()[0].kind_ == parameters::EventKind::kReset);
        Check(Output(result, 3).scalar_ == 0);

        document.nodes_[0] = registry.MakeNode(1, "event.cue");
        document.nodes_[1] = registry.MakeNode(2, "event.merge");
        document.nodes_[2] = registry.MakeNode(3, "event.step");
        document.nodes_.back() = registry.MakeNode(7, "control.scalar");
        document.edges_ = {{1, 1, 2, "a"},      {2, 1, 2, "b"},      {3, 2, 3, "events"},
                           {4, 3, 4, "amount"}, {5, 4, 5, "source"}, {6, 2, 6, "events"}};
        document.control_snapshots_ = {{1, "Trigger cue", {{7, 0.5}}}};
        document.control_cues_ = {{1, "First", 0.2, 1}, {2, "Second", 0.4, 1}};
        ++context.reset_generation_;
        evaluate(0);
        result = evaluate(0.45);
        Check(Output(result, 3).scalar_ == 2 && Output(result, 1).events_->Events().size() == 2);
        Check(Output(evaluate(0.45), 3).scalar_ == 2);
        std::cout << "Runtime beat/audio sources, fanout dedup, envelope, edits, pause and reset "
                     "passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
