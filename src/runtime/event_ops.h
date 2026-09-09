#pragma once

#include "rhythm/parameters/event_envelope.h"
#include "rhythm/runtime/runtime.h"

namespace rhythm::runtime::detail {
bool IsEventOperation(graph::Operation operation);
struct EventEvaluation {
    double scalar_ = 0;
    std::shared_ptr<const parameters::EventBatch> events_{};
    std::size_t rejected_ = 0;
    std::optional<EventObservation> observation_{};
};
// One node's event production/consumption state. No renderer, UI, worker or clock.
// Inputs are already evaluated, immutable outputs in the acyclic execution plan.
class EventNode final {
   public:
    EventEvaluation Evaluate(const graph::Instruction& instruction,
                             std::span<const NodeOutput> outputs, const FrameContext& frame,
                             const graph::ExecutionPlan& plan, std::uint64_t& next_sequence);

   private:
    std::optional<graph::Node> node_{};
    std::optional<parameters::BeatSettings> grid_{};
    std::optional<double> previous_{};
    std::uint64_t generation_ = 0;
    std::uint64_t audio_generation_ = 0;
    std::uint64_t onset_ = 0;
    std::map<parameters::EventSource, std::uint64_t> seen_{};
    parameters::EventEnvelope envelope_{};
    double scalar_ = 0;
    double last_event_seconds_ = 0;
    bool high_ = false;
    bool started_ = false;
    EventObservation observation_{};
};
}  // namespace rhythm::runtime::detail
