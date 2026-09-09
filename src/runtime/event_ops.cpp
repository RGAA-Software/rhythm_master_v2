#include "event_ops.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace rhythm::runtime::detail {
bool IsEventOperation(graph::Operation operation) {
    return operation >= graph::Operation::kEventBeat && operation <= graph::Operation::kEventInput;
}
EventEvaluation EventNode::Evaluate(const graph::Instruction& instruction,
                                    std::span<const NodeOutput> outputs, const FrameContext& frame,
                                    const graph::ExecutionPlan& plan,
                                    std::uint64_t& next_sequence) {
    using namespace parameters;
    using Operation = graph::Operation;
    const auto& node = instruction.node_;
    const auto operation = instruction.operation_;
    const auto property = [&](const char* key, double fallback) {
        return graph::Scalar(node, key, fallback);
    };
    const auto input = [&](std::size_t port) -> const NodeOutput& {
        return outputs[instruction.inputs_.at(port).value()];
    };
    if (frame.reset_generation_ == std::numeric_limits<std::uint64_t>::max())
        throw std::invalid_argument("event.generation");
    const auto generation = frame.reset_generation_ + 1;
    const bool changed = !node_ || *node_ != node || grid_ != plan.beat_grid_ ||
                         generation_ != generation || (previous_ && frame.seconds_ < *previous_);
    if (changed) {
        node_ = node;
        grid_ = plan.beat_grid_;
        generation_ = generation;
        previous_.reset();
        started_ = false;
        seen_.clear();
        scalar_ = property("initial", 0);
        high_ = scalar_ != 0;
        last_event_seconds_ = frame.seconds_;
        audio_generation_ = frame.external_.audio_ ? frame.external_.audio_->generation_ : 0;
        onset_ = frame.external_.audio_ ? frame.external_.audio_->onset_id_ : 0;
        envelope_ = EventEnvelope({property("attack", 0.01), property("decay", 0.1),
                                   property("sustain", 0.7), property("release", 0.3),
                                   property("duration", 0.3)});
        envelope_.Reset(generation, frame.seconds_);
    }
    EventEvaluation result;
    EventBatch produced;
    const auto emit = [&](double seconds, EventOrigin origin, EventKind kind, double value) {
        if (next_sequence == std::numeric_limits<std::uint64_t>::max() ||
            !produced.Append(
                    {seconds, {0, node.id_, origin}, next_sequence, generation, kind, value})) {
            ++result.rejected_;
            return false;
        }
        ++next_sequence;
        return true;
    };
    if (frame.advance_state_) {
        if (operation == Operation::kEventInput) {
            const auto& track = std::get<EventTrack>(node.properties_.at("actions"));
            std::vector<Event> events;
            // A paused observation at zero must not consume zero-time actions.
            // First evaluation after seeking elsewhere does not replay history.
            if (!started_ && previous_.value_or(frame.seconds_) == 0)
                for (const auto& action : track.Events()) {
                    if (action.seconds_ != 0) break;
                    events.push_back({0,
                                      {0, node.id_, EventOrigin::kOperator},
                                      action.id_,
                                      generation,
                                      action.kind_,
                                      action.value_});
                }
            if (previous_)
                for (const auto& action : track.Between(*previous_, frame.seconds_))
                    events.push_back({action.seconds_,
                                      {0, node.id_, EventOrigin::kOperator},
                                      action.id_,
                                      generation,
                                      action.kind_,
                                      action.value_});
            if (frame.external_.events_)
                for (const auto& event : frame.external_.events_->Events()) {
                    if (event.source_.node_ != node.id_) continue;
                    if (event.generation_ != generation || event.seconds_ > frame.seconds_) {
                        ++result.rejected_;
                        continue;
                    }
                    auto& sequence = seen_[event.source_];
                    if (event.sequence_ <= sequence) continue;
                    sequence = event.sequence_;
                    auto dispatched = event;
                    // Record the time at which the action actually reached the
                    // graph, including bounded host-queue delay.
                    dispatched.seconds_ = frame.seconds_;
                    events.push_back(dispatched);
                }
            std::sort(events.begin(), events.end(), EventBefore);
            for (const auto& event : events)
                emit(event.seconds_, event.source_.origin_, event.kind_, event.value_);
            started_ = true;
        } else if (operation == Operation::kEventBeat && previous_ && plan.beat_grid_) {
            const BeatGrid grid(*plan.beat_grid_);
            auto next = grid.NextAfter(*previous_, Quantization::kBeat);
            const auto interval = static_cast<std::int64_t>(property("beat_interval", 1));
            // At most 256 emitted events and 64 candidate beats per event. A
            // discontinuity must reset the source, not cause an unbounded loop.
            std::size_t candidates = 0;
            while (next && *next <= frame.seconds_ && candidates++ < EventBatch::kCapacity * 64) {
                if (grid.Position(*next).beat_ % interval == 0 &&
                    !emit(*next, EventOrigin::kBeat, EventKind::kPulse, 1))
                    break;
                next = grid.NextAfter(*next, Quantization::kBeat);
            }
            if (next && *next <= frame.seconds_ && candidates >= EventBatch::kCapacity * 64)
                ++result.rejected_;
        } else if (operation == Operation::kEventCue && previous_ && plan.control_sequence_) {
            for (const auto& cue : plan.control_sequence_->Cues())
                if (cue.seconds_ > *previous_ && cue.seconds_ <= frame.seconds_)
                    emit(cue.seconds_, EventOrigin::kCue, EventKind::kPulse, 1);
        } else if (operation == Operation::kEventAudio && frame.external_.audio_ &&
                   frame.external_.audio_->valid_) {
            const auto& audio = *frame.external_.audio_;
            if (audio_generation_ != audio.generation_) {
                audio_generation_ = audio.generation_;
                onset_ = audio.onset_id_;
            } else if (audio.onset_id_ > onset_) {
                onset_ = audio.onset_id_;
                const auto first = static_cast<std::size_t>(property("band_first", 0));
                const auto last = static_cast<std::size_t>(property("band_last", 62));
                const auto peak = *std::max_element(audio.mono_bands_.begin() + first,
                                                    audio.mono_bands_.begin() + last + 1);
                if (peak >= property("threshold", 0.05))
                    emit(frame.seconds_, EventOrigin::kAudio, EventKind::kPulse, peak);
            }
        } else if (operation == Operation::kEventEdge) {
            const auto value = input(0).scalar_;
            const bool high =
                    high_ ? value > property("threshold", 0.5) - property("hysteresis", 0.01)
                          : value >= property("threshold", 0.5);
            if (high != high_) {
                high_ = high;
                if (property("edge_mode", 0) == 1)
                    emit(frame.seconds_, EventOrigin::kOperator, EventKind::kGate, high ? 1 : 0);
                else if (high)
                    emit(frame.seconds_, EventOrigin::kOperator, EventKind::kPulse, 1);
            }
        } else if (operation >= Operation::kEventMerge) {
            std::vector<Event> events;
            const auto collect = [&](std::size_t port) {
                if (input(port).events_) {
                    const auto batch = input(port).events_->Events();
                    events.insert(events.end(), batch.begin(), batch.end());
                }
            };
            collect(0);
            if (operation == Operation::kEventMerge) collect(1);
            std::sort(events.begin(), events.end(), EventBefore);
            for (const auto& event : events) {
                const auto found = seen_.find(event.source_);
                if (event.generation_ != generation || !ValidEvent(event)) {
                    ++result.rejected_;
                    continue;
                }
                if (found != seen_.end() && event.sequence_ <= found->second) continue;
                if ((found == seen_.end() && seen_.size() == EventQueue::kSourceCapacity) ||
                    event.seconds_ < last_event_seconds_ || event.seconds_ > frame.seconds_) {
                    ++result.rejected_;
                    continue;
                }
                seen_[event.source_] = event.sequence_;
                last_event_seconds_ = event.seconds_;
                if (operation == Operation::kEventMerge) {
                    if (!produced.Append(event)) ++result.rejected_;
                } else if (operation == Operation::kEventReset) {
                    if (event.kind_ != EventKind::kGate || event.value_ != 0)
                        emit(event.seconds_, EventOrigin::kOperator, EventKind::kReset, 1);
                } else if (operation == Operation::kEventEnvelope) {
                    envelope_.Apply(event);
                } else if (event.kind_ == EventKind::kReset) {
                    scalar_ = property("initial", 0);
                    high_ = scalar_ != 0;
                } else if (operation == Operation::kEventGate) {
                    high_ = event.kind_ == EventKind::kGate ? event.value_ != 0 : !high_;
                } else if (event.kind_ != EventKind::kGate || event.value_ != 0) {
                    if (operation == Operation::kEventLatch) scalar_ = input(1).scalar_;
                    if (operation == Operation::kEventStep) {
                        const auto steps = static_cast<std::int64_t>(property("steps", 8));
                        const auto next = static_cast<std::int64_t>(scalar_) +
                                          static_cast<std::int64_t>(property("step", 1));
                        scalar_ = static_cast<double>((next % steps + steps) % steps);
                    }
                }
            }
        }
    }
    previous_ = frame.seconds_;
    if (operation == Operation::kEventEnvelope) scalar_ = envelope_.Sample(frame.seconds_);
    result.scalar_ = operation == Operation::kEventGate ? (high_ ? input(1).scalar_ : 0) : scalar_;
    if (!produced.Events().empty()) {
        observation_.count_ += produced.Events().size();
        observation_.last_sequence_ = produced.Events().back().sequence_;
        observation_.last_seconds_ = produced.Events().back().seconds_;
        result.events_ = std::make_shared<const EventBatch>(std::move(produced));
    }
    if (operation <= Operation::kEventMerge || operation == Operation::kEventReset ||
        operation == Operation::kEventInput)
        result.observation_ = observation_;
    return result;
}
}  // namespace rhythm::runtime::detail
