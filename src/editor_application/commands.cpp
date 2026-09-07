#include "rhythm/editor/commands.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

#include "rhythm/graph/bindings.h"

namespace rhythm::editor {
EditResult AddNode(const Snapshot& snapshot, const graph::Registry& registry, std::string_view type,
                   Position position, graph::NodeId id) {
    if (!registry.Find(type, snapshot.document_.components_))
        return graph::Diagnostic{"graph.unsupported_node"};
    if (!std::isfinite(position.x_) || !std::isfinite(position.y_))
        return graph::Diagnostic{"project.layout"};
    if (snapshot.document_.nodes_.size() >= 10000) return graph::Diagnostic{"graph.limit"};
    if (!id) return graph::Diagnostic{"graph.duplicate_node"};
    for (const auto& node : snapshot.document_.nodes_) {
        if (node.id_ == id) return graph::Diagnostic{"graph.duplicate_node", id};
    }
    auto next = snapshot;
    next.document_.nodes_.push_back(registry.MakeNode(id, type, snapshot.document_.components_));
    next.positions_[id] = position;
    if (!next.document_.output_ &&
        registry.Find(type, snapshot.document_.components_)->operation_ ==
                graph::Operation::kOutput)
        next.document_.output_ = id;
    return next;
}
EditResult Connect(const Snapshot& snapshot, const graph::Registry& registry, graph::NodeId from,
                   graph::NodeId to, std::string_view input) {
    const auto find_node = [&](graph::NodeId id) {
        return std::find_if(snapshot.document_.nodes_.begin(), snapshot.document_.nodes_.end(),
                            [&](const auto& node) { return node.id_ == id; });
    };
    const auto source = find_node(from);
    const auto destination = find_node(to);
    if (source == snapshot.document_.nodes_.end() || destination == snapshot.document_.nodes_.end())
        return graph::Diagnostic{"graph.missing_node", to};
    const auto source_type = registry.Find(source->type_, snapshot.document_.components_);
    const auto destination_type = registry.Find(destination->type_, snapshot.document_.components_);
    if (!source_type || !destination_type) return graph::Diagnostic{"graph.unsupported_node", to};
    const auto port =
            std::find_if(destination_type->inputs_.begin(), destination_type->inputs_.end(),
                         [&](const auto& item) { return item.key_ == input; });
    if (port == destination_type->inputs_.end()) return graph::Diagnostic{"graph.missing_port", to};
    if (port->type_ != source_type->output_) return graph::Diagnostic{"graph.port_type", to};
    auto next = snapshot;
    std::erase_if(next.document_.edges_,
                  [&](const auto& edge) { return edge.to_ == to && edge.input_ == input; });
    std::erase_if(next.document_.bindings_, [&](const auto& binding) {
        return binding.node_ == to && binding.input_ == input;
    });
    if (next.document_.edges_.size() >= 40000) return graph::Diagnostic{"graph.limit"};
    std::uint64_t id = 1;
    for (const auto& edge : snapshot.document_.edges_) {
        if (edge.id_ == std::numeric_limits<std::uint64_t>::max())
            return graph::Diagnostic{"graph.id_exhausted"};
        id = std::max(id, edge.id_ + 1);
    }
    // Check the proposed edge even when unrelated nodes are incomplete. A full
    // compile may stop at missing output/ports before reaching cycle validation.
    if (destination_type->operation_ != graph::Operation::kFeedback) {
        std::map<graph::NodeId, std::vector<graph::NodeId>> dependents;
        std::set<graph::NodeId> feedback;
        for (const auto& node : next.document_.nodes_)
            if (const auto descriptor = registry.Find(node.type_, next.document_.components_);
                descriptor && descriptor->operation_ == graph::Operation::kFeedback)
                feedback.insert(node.id_);
        const auto resolved = graph::ResolveEdges(next.document_);
        if (std::holds_alternative<std::vector<graph::Diagnostic>>(resolved))
            return std::get<std::vector<graph::Diagnostic>>(resolved).front();
        for (const auto& edge : std::get<std::vector<graph::Edge>>(resolved))
            if (!feedback.contains(edge.to_)) dependents[edge.from_].push_back(edge.to_);
        std::vector<graph::NodeId> pending{to};
        std::set<graph::NodeId> visited;
        while (!pending.empty()) {
            const auto current = pending.back();
            pending.pop_back();
            if (current == from) return graph::Diagnostic{"graph.cycle", to};
            if (!visited.insert(current).second) continue;
            const auto found = dependents.find(current);
            if (found != dependents.end())
                pending.insert(pending.end(), found->second.begin(), found->second.end());
        }
    }
    next.document_.edges_.push_back({id, from, to, std::string(input)});
    return next;
}
}  // namespace rhythm::editor
