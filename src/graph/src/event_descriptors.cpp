#include "event_descriptors.h"

namespace rhythm::graph {
void AppendEventDescriptors(std::vector<OperatorDescriptor>& operators) {
    using Type = ValueType;
    operators.push_back({"event.input",
                         Operation::kEventInput,
                         Type::kEvent,
                         {},
                         {{"actions", parameters::EventTrack{}}},
                         true});
    operators.push_back({"event.beat",
                         Operation::kEventBeat,
                         Type::kEvent,
                         {},
                         {{"beat_interval", 1.0, 1, 64, {}, true}},
                         true});
    operators.push_back({"event.cue", Operation::kEventCue, Type::kEvent, {}, {}, true});
    operators.push_back({"event.audio_onset",
                         Operation::kEventAudio,
                         Type::kEvent,
                         {},
                         {{"threshold", 0.05, 0, 1},
                          {"band_first", 0.0, 0, 62, {}, true},
                          {"band_last", 62.0, 0, 62, {}, true}},
                         true});
    operators.push_back({"event.edge",
                         Operation::kEventEdge,
                         Type::kEvent,
                         {{"value", Type::kScalar}},
                         {{"threshold", 0.5, -1e6, 1e6},
                          {"hysteresis", 0.01, 0, 1e6},
                          {"edge_mode", 0.0, 0, 1, {"event.rising", "event.gate_edges"}, true}},
                         true});
    operators.push_back({"event.merge",
                         Operation::kEventMerge,
                         Type::kEvent,
                         {{"a", Type::kEvent}, {"b", Type::kEvent}},
                         {},
                         true});
    operators.push_back({"event.envelope",
                         Operation::kEventEnvelope,
                         Type::kScalar,
                         {{"events", Type::kEvent}},
                         {{"attack", 0.01, 0, 86400},
                          {"decay", 0.1, 0, 86400},
                          {"sustain", 0.7, 0, 1},
                          {"release", 0.3, 0, 86400},
                          {"duration", 0.3, 0, 86400}},
                         true});
    operators.push_back({"event.step",
                         Operation::kEventStep,
                         Type::kScalar,
                         {{"events", Type::kEvent}},
                         {{"steps", 8.0, 1, 65536, {}, true},
                          {"step", 1.0, -65536, 65536, {}, true},
                          {"initial", 0.0, 0, 65535, {}, true}},
                         true});
    operators.push_back({"event.gate",
                         Operation::kEventGate,
                         Type::kScalar,
                         {{"events", Type::kEvent}, {"value", Type::kScalar}},
                         {{"initial", 0.0, 0, 1, {}, true}},
                         true});
    operators.push_back({"event.latch",
                         Operation::kEventLatch,
                         Type::kScalar,
                         {{"events", Type::kEvent}, {"value", Type::kScalar}},
                         {{"initial", 0.0, -1e6, 1e6}},
                         true});
    operators.push_back({"event.reset",
                         Operation::kEventReset,
                         Type::kEvent,
                         {{"events", Type::kEvent}},
                         {},
                         true});
}
}  // namespace rhythm::graph
