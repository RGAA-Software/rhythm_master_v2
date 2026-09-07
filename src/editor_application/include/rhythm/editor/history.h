#pragma once

#include "rhythm/assets/types.h"
#include "rhythm/graph/compiler.h"
#include "rhythm/media/soundtrack.h"

namespace rhythm::editor {
struct Position {
    float x_ = 0;
    float y_ = 0;
    bool operator==(const Position&) const = default;
};
struct Snapshot {
    graph::Document document_{};
    std::map<graph::NodeId, Position> positions_{};
    std::string title_{};
    std::vector<assets::AssetRecord> assets_{};
    std::map<std::string, std::map<graph::NodeId, Position>> component_positions_{};
    std::optional<media::Soundtrack> soundtrack_{};
    bool operator==(const Snapshot&) const = default;
};
// Editing may contain incomplete graphs. Validation prevents executing them,
// while undo remains available. Each accepted edit receives a monotonic revision.
class History final {
   public:
    explicit History(Snapshot initial);
    const Snapshot& Current() const { return current_; }
    bool Apply(Snapshot next, std::uint64_t expected_revision);
    bool Undo();
    bool Redo();
    // IDs reserved during this editing session are never reused after undo.
    graph::NodeId ReserveNodeId();

   private:
    void Install(Snapshot snapshot);
    Snapshot current_{};
    std::vector<Snapshot> undo_{};
    std::vector<Snapshot> redo_{};
    std::uint64_t revision_ = 0;
    graph::NodeId next_node_id_ = 1;
};
}  // namespace rhythm::editor
