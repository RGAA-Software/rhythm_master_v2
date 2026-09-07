#include <algorithm>

#include "rhythm/editor/commands.h"

namespace rhythm::editor {
EditResult DetachComponent(const Snapshot& snapshot, const graph::Registry& registry,
                           graph::NodeId instance, std::string_view unique_type) {
    if (!unique_type.starts_with("component.user.") || unique_type.size() > 200)
        return graph::Diagnostic{"graph.component", instance};
    const auto& document = snapshot.document_;
    const auto node = std::find_if(document.nodes_.begin(), document.nodes_.end(),
                                   [&](const auto& value) { return value.id_ == instance; });
    if (node == document.nodes_.end()) return graph::Diagnostic{"graph.missing_node", instance};
    const auto descriptor = registry.Find(node->type_, document.components_);
    if (!descriptor || descriptor->operation_ != graph::Operation::kComponent)
        return graph::Diagnostic{"graph.component", instance};
    std::vector<std::string> pending{node->type_};
    std::map<std::string, std::string> names{{node->type_, std::string(unique_type)}};
    std::vector<graph::ComponentDefinition> copies;
    for (std::size_t index = 0; index < pending.size(); ++index) {
        if (document.components_.size() + pending.size() > 256)
            return graph::Diagnostic{"graph.limit", instance};
        const auto found =
                std::find_if(document.components_.begin(), document.components_.end(),
                             [&](const auto& value) { return value.type_ == pending[index]; });
        if (found == document.components_.end())
            return graph::Diagnostic{"graph.component", instance};
        auto copy = *found;
        copy.type_ = names.at(pending[index]);
        if (std::any_of(document.components_.begin(), document.components_.end(),
                        [&](const auto& value) { return value.type_ == copy.type_; }))
            return graph::Diagnostic{"graph.component", instance};
        for (auto& child : copy.nodes_) {
            if (std::none_of(document.components_.begin(), document.components_.end(),
                             [&](const auto& value) { return value.type_ == child.type_; }))
                continue;
            if (!names.contains(child.type_)) {
                names[child.type_] =
                        std::string(unique_type) + ".nested" + std::to_string(pending.size());
                pending.push_back(child.type_);
            }
            child.type_ = names.at(child.type_);
        }
        copies.push_back(std::move(copy));
    }
    auto next = snapshot;
    for (const auto& [source, destination] : names)
        if (const auto layout = snapshot.component_positions_.find(source);
            layout != snapshot.component_positions_.end())
            next.component_positions_[destination] = layout->second;
    for (auto& copy : copies) next.document_.components_.push_back(std::move(copy));
    std::size_t stored_nodes = next.document_.nodes_.size();
    std::size_t stored_edges = next.document_.edges_.size() + next.document_.bindings_.size();
    for (const auto& definition : next.document_.components_) {
        stored_nodes += definition.nodes_.size();
        stored_edges += definition.edges_.size() + definition.bindings_.size();
    }
    if (stored_nodes > 10000 || stored_edges > 40000)
        return graph::Diagnostic{"graph.limit", instance};
    for (auto& value : next.document_.nodes_)
        if (value.id_ == instance) value.type_ = unique_type;
    if (!registry.Find(unique_type, next.document_.components_))
        return graph::Diagnostic{"graph.component", instance};
    return next;
}
}  // namespace rhythm::editor
