#pragma once

#include "rhythm/editor/commands.h"

namespace rhythm::editor {
struct TransformDriver {
    std::string property_{};
    graph::NodeId source_ = 0;
    std::string source_type_{};
    std::string signal_{};
    std::optional<graph::NodeId> curve_clock_{};
    double minimum_ = 0;
    double maximum_ = 0;
};
using TransformDrivers = std::variant<std::vector<TransformDriver>, graph::Diagnostic>;
TransformDrivers InspectTransformDrivers(const graph::Document& document,
                                         const graph::Registry& registry, graph::NodeId selected);
// UI-thread observation of one accepted runtime frame, with authored identity.
// Only requested scalar sources/clocks need inclusion; no backend types or clock
// ownership are introduced. The host must additionally check plan generation.
struct TransformSample {
    std::string document_id_{};
    std::uint64_t revision_ = 0;
    std::map<graph::NodeId, double> scalars_{};
};
// Explicit author action: detach this transform's scalar controls and retain
// their observed, clamped values. Other edges, signal definitions and producers
// are preserved. One value result, suitable for one undo transaction.
EditResult FreezeTransformDrivers(const Snapshot& snapshot, const graph::Registry& registry,
                                  graph::NodeId selected, const TransformSample& sample);
// Explicit shared-source edit. Writes at the curve's observed INPUT time, which
// can be a local/loop clock, not necessarily global transport seconds. Only a
// direct authored scalar.curve source is supported; no expression inversion.
EditResult RecordTransformKey(const Snapshot& snapshot, const graph::Registry& registry,
                              graph::NodeId selected, const std::string& property, double value,
                              const TransformSample& sample);
}  // namespace rhythm::editor
