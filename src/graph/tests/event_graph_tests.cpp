#include <algorithm>
#include <iostream>
#include <set>
#include <stdexcept>

#include "rhythm/graph/compiler.h"
#include "rhythm/graph/components.h"

namespace {
void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
}  // namespace
int main() {
    using namespace rhythm::graph;
    try {
        Registry registry;
        Document graph;
        graph.id_ = "event-graph";
        graph.beat_grid_ = rhythm::parameters::BeatSettings{};
        graph.nodes_ = {registry.MakeNode(1, "event.beat"), registry.MakeNode(2, "event.envelope"),
                        registry.MakeNode(3, "texture.gradient"),
                        registry.MakeNode(4, "output.texture")};
        graph.edges_ = {{1, 1, 2, "events"}, {2, 2, 3, "amount"}, {3, 3, 4, "source"}};
        graph.output_ = 4;
        auto result = Compile(graph, registry);
        Check(std::holds_alternative<ExecutionPlan>(result), "typed beat/envelope graph failed");
        Check(std::get<ExecutionPlan>(result).instructions_.size() == 4,
              "event path lost instructions");
        auto invalid = graph;
        invalid.beat_grid_.reset();
        Check(std::holds_alternative<std::vector<Diagnostic>>(Compile(invalid, registry)),
              "beat source silently invented a grid");
        invalid = graph;
        invalid.edges_[1].from_ = 1;
        Check(std::holds_alternative<std::vector<Diagnostic>>(Compile(invalid, registry)),
              "event implicitly converted to continuous scalar");
        invalid = graph;
        invalid.nodes_.push_back(registry.MakeNode(5, "event.merge"));
        invalid.nodes_.push_back(registry.MakeNode(6, "event.merge"));
        invalid.edges_.insert(invalid.edges_.end(),
                              {{4, 6, 5, "a"}, {5, 1, 5, "b"}, {6, 5, 6, "a"}, {7, 1, 6, "b"}});
        invalid.edges_[0].from_ = 5;
        const auto loop = Compile(invalid, registry);
        Check(std::holds_alternative<std::vector<Diagnostic>>(loop),
              "immediate event recursion accepted");

        ComponentDefinition envelope;
        envelope.type_ = "component.event_envelope";
        envelope.nodes_ = {registry.MakeNode(1, "event.envelope")};
        envelope.output_ = 1;
        envelope.inputs_ = {{"trigger", 1, "events"}};
        envelope.parameters_ = {{"attack", 1, "attack", "Envelope"}};
        graph.components_ = {envelope};
        graph.nodes_[1] = registry.MakeNode(2, envelope.type_, graph.components_);
        graph.edges_[0].input_ = "trigger";
        const auto descriptor = registry.Find(envelope.type_, graph.components_);
        Check(descriptor && descriptor->inputs_[0].type_ == ValueType::kEvent &&
                      descriptor->output_ == ValueType::kScalar,
              "component event boundary type lost");
        result = Compile(graph, registry);
        Check(std::holds_alternative<ExecutionPlan>(result), "component event expansion failed");
        const auto& plan = std::get<ExecutionPlan>(result);
        const auto expanded = std::find_if(
                plan.instructions_.begin(), plan.instructions_.end(),
                [](const auto& item) { return item.operation_ == Operation::kEventEnvelope; });
        // The component output deliberately inherits its instance ID (2), so
        // existing downstream edges retain their stable target after expansion.
        Check(expanded != plan.instructions_.end() && expanded->node_.id_ == 2 &&
                      plan.instructions_.at(*expanded->inputs_[0]).node_.id_ == 1,
              "expanded event source reference lost");
        auto step = registry.MakeNode(10, "event.step");
        step.properties_["initial"] = 8.0;
        Check(!registry.ValidateNode(step).empty(), "invalid initial step accepted");
        auto audio = registry.MakeNode(11, "event.audio_onset");
        audio.properties_["band_first"] = 40.0;
        audio.properties_["band_last"] = 20.0;
        Check(!registry.ValidateNode(audio).empty(), "reversed onset bands accepted");
        graph.components_.clear();
        graph.nodes_[0] = registry.MakeNode(1, "event.cue");
        graph.nodes_[1] = registry.MakeNode(2, "event.envelope");
        graph.edges_[0].input_ = "events";
        graph.nodes_.push_back(registry.MakeNode(9, "control.scalar"));
        graph.control_snapshots_ = {{1, "Cue source", {{9, 0.5}}}};
        graph.control_cues_ = {{1, "First", 0.5, 1}};
        result = Compile(graph, registry);
        Check(std::holds_alternative<ExecutionPlan>(result) &&
                      std::get<ExecutionPlan>(result).control_sequence_.has_value() &&
                      std::get<ExecutionPlan>(result).instructions_.size() == 5,
              "Cue events lost a bank without continuous macro connections");
        std::cout << "Event port separation, component references, property ranges and cycle "
                     "rejection passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
