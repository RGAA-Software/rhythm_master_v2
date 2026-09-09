#include <algorithm>
#include <limits>
#include <set>

#include "rhythm/editor/commands.h"

namespace rhythm::editor {
EditResult InstantiateTemplate(const Snapshot& snapshot, const Snapshot& content,
                               std::span<const graph::NodeId> node_ids) {
    if (node_ids.size() != content.document_.nodes_.size() || node_ids.size() > 10000)
        return graph::Diagnostic{"graph.limit"};
    if (!std::holds_alternative<graph::ExecutionPlan>(
                graph::Compile(content.document_, graph::Registry{})))
        return graph::Diagnostic{"graph.template_invalid"};
    std::set<graph::NodeId> used;
    for (const auto& node : snapshot.document_.nodes_) used.insert(node.id_);
    std::map<graph::NodeId, graph::NodeId> remap;
    for (std::size_t index = 0; index < node_ids.size(); ++index) {
        if (!node_ids[index] || !used.insert(node_ids[index]).second)
            return graph::Diagnostic{"graph.duplicate_node"};
        remap.emplace(content.document_.nodes_[index].id_, node_ids[index]);
    }
    auto next = content;
    next.document_.id_ = snapshot.document_.id_;
    next.document_.revision_ = snapshot.document_.revision_;
    for (auto& node : next.document_.nodes_) node.id_ = remap.at(node.id_);
    next.document_.output_ = remap.at(next.document_.output_);
    for (auto& signal : next.document_.signals_) signal.source_ = remap.at(signal.source_);
    for (auto& binding : next.document_.bindings_) binding.node_ = remap.at(binding.node_);
    next.document_.control_titles_.clear();
    for (const auto& [id, title] : content.document_.control_titles_)
        next.document_.control_titles_.emplace(remap.at(id), title);
    for (auto& control_snapshot : next.document_.control_snapshots_) {
        parameters::ControlValues values;
        for (const auto& [id, value] : control_snapshot.values_)
            values.emplace(remap.at(id), value);
        control_snapshot.values_ = std::move(values);
    }
    // Cue targets identify snapshots, not graph nodes; their identities remain
    // unchanged when the entire template replaces the current graph.
    std::uint64_t edge_id = 0;
    for (const auto& edge : snapshot.document_.edges_) edge_id = std::max(edge_id, edge.id_);
    if (next.document_.edges_.size() > std::numeric_limits<std::uint64_t>::max() - edge_id)
        return graph::Diagnostic{"graph.id_exhausted"};
    for (auto& edge : next.document_.edges_) {
        edge.id_ = ++edge_id;
        edge.from_ = remap.at(edge.from_);
        edge.to_ = remap.at(edge.to_);
    }
    next.positions_.clear();
    for (const auto& [id, position] : content.positions_)
        if (remap.contains(id)) next.positions_.emplace(remap.at(id), position);
    // A valid source template must remain executable after identity remapping.
    // Reject the edit before history/authoring can diverge from the live output.
    if (!std::holds_alternative<graph::ExecutionPlan>(
                graph::Compile(next.document_, graph::Registry{})))
        return graph::Diagnostic{"graph.template_invalid"};
    return next;
}
}  // namespace rhythm::editor
