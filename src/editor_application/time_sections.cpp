#include <algorithm>
#include <cmath>

#include "rhythm/editor/commands.h"

namespace rhythm::editor {
EditResult AddTimeSection(const Snapshot& snapshot, const graph::Registry& registry, double start,
                          graph::NodeId section_id, graph::NodeId clock_id) {
    if (!std::isfinite(start) || start < 0 || start > 86400)
        return graph::Diagnostic{"graph.property_range", section_id, "clip_start"};
    auto next = snapshot;
    Position position{0, 0};
    for (const auto& [id, placed] : next.positions_)
        position.y_ = std::max(position.y_, placed.y_ + 200);
    const auto clock = std::find_if(next.document_.nodes_.begin(), next.document_.nodes_.end(),
                                    [](const auto& node) { return node.type_ == "core.time"; });
    if (clock != next.document_.nodes_.end()) {
        clock_id = clock->id_;
    } else {
        auto added = AddNode(next, registry, "core.time", position, clock_id);
        if (std::holds_alternative<graph::Diagnostic>(added)) return added;
        next = std::move(std::get<Snapshot>(added));
    }
    position.x_ += 260;
    auto added = AddNode(next, registry, "time.envelope", position, section_id);
    if (std::holds_alternative<graph::Diagnostic>(added)) return added;
    next = std::move(std::get<Snapshot>(added));
    next.document_.nodes_.back().properties_["clip_start"] = start;
    return Connect(next, registry, clock_id, section_id, "time");
}
}  // namespace rhythm::editor
