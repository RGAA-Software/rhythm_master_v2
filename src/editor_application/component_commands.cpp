#include <algorithm>
#include <set>

#include "rhythm/editor/commands.h"
#include "rhythm/graph/bindings.h"
#include "rhythm/graph/components.h"

namespace rhythm::editor {
EditResult MakeComponent(const Snapshot& snapshot, const graph::Registry& registry,
                         std::span<const graph::NodeId> selection, graph::NodeId instance,
                         std::string_view title) {
    const auto& document = snapshot.document_;
    if (selection.empty() || selection.size() > 10000 || document.components_.size() >= 256 ||
        title.empty() || title.size() > 256)
        return graph::Diagnostic{"graph.component"};
    if (!instance || std::any_of(document.nodes_.begin(), document.nodes_.end(),
                                 [&](const auto& node) { return node.id_ == instance; }))
        return graph::Diagnostic{"graph.duplicate_node", instance};
    const std::set<graph::NodeId> selected(selection.begin(), selection.end());
    graph::ComponentDefinition definition;
    definition.type_ = "component.user." + std::to_string(instance);
    definition.title_ = title;
    if (std::any_of(document.components_.begin(), document.components_.end(),
                    [&](const auto& value) { return value.type_ == definition.type_; }))
        return graph::Diagnostic{"graph.component"};
    for (const auto& node : document.nodes_) {
        if (!selected.contains(node.id_)) continue;
        if (node.type_ == "control.scalar") return graph::Diagnostic{"control.root_only"};
        const auto validation = registry.ValidateNode(node, document.components_);
        if (!validation.empty()) return validation.front();
        definition.nodes_.push_back(node);
    }
    if (definition.nodes_.size() != selected.size()) return graph::Diagnostic{"graph.missing_node"};
    const auto resolved = graph::ResolveEdges(document);
    if (std::holds_alternative<std::vector<graph::Diagnostic>>(resolved))
        return std::get<std::vector<graph::Diagnostic>>(resolved).front();
    std::set<graph::NodeId> outputs;
    if (selected.contains(document.output_)) outputs.insert(document.output_);
    for (const auto& edge : std::get<std::vector<graph::Edge>>(resolved))
        if (selected.contains(edge.from_) && !selected.contains(edge.to_))
            outputs.insert(edge.from_);
    for (const auto& signal : document.signals_)
        if (selected.contains(signal.source_)) outputs.insert(signal.source_);
    if (outputs.size() > 1) return graph::Diagnostic{"graph.component_outputs"};
    definition.output_ = outputs.empty() ? selection.front() : *outputs.begin();
    std::map<std::pair<graph::NodeId, std::string>, std::string> exposed;
    std::set<std::string> input_names;
    const auto expose = [&](graph::NodeId node, const std::string& input) {
        const auto target = std::make_pair(node, input);
        if (const auto found = exposed.find(target); found != exposed.end()) return found->second;
        std::string name = input.size() <= 112 ? input : "input";
        const auto base = name;
        for (std::size_t suffix = 2; input_names.contains(name); ++suffix)
            name = base + "_" + std::to_string(suffix);
        input_names.insert(name);
        exposed.emplace(target, name);
        definition.inputs_.push_back({name, node, input});
        return name;
    };
    auto next = snapshot;
    next.document_.edges_.clear();
    for (auto edge : document.edges_) {
        const bool from = selected.contains(edge.from_);
        const bool to = selected.contains(edge.to_);
        if (from && to) {
            definition.edges_.push_back(std::move(edge));
            continue;
        }
        if (to) {
            edge.input_ = expose(edge.to_, edge.input_);
            edge.to_ = instance;
        }
        if (from) edge.from_ = instance;
        next.document_.edges_.push_back(std::move(edge));
    }
    next.document_.bindings_.clear();
    for (auto binding : document.bindings_) {
        if (selected.contains(binding.node_)) {
            const auto signal =
                    std::find_if(document.signals_.begin(), document.signals_.end(),
                                 [&](const auto& value) { return value.name_ == binding.signal_; });
            if (signal == document.signals_.end())
                return graph::Diagnostic{"graph.binding_missing"};
            if (selected.contains(signal->source_)) {
                definition.bindings_.push_back(std::move(binding));
                continue;
            }
            binding.input_ = expose(binding.node_, binding.input_);
            binding.node_ = instance;
        }
        next.document_.bindings_.push_back(std::move(binding));
    }
    next.document_.signals_.clear();
    for (auto signal : document.signals_) {
        if (selected.contains(signal.source_)) {
            definition.signals_.push_back(signal);
            if (signal.source_ != definition.output_) continue;
            signal.source_ = instance;
        }
        next.document_.signals_.push_back(std::move(signal));
    }
    const auto output =
            std::find_if(definition.nodes_.begin(), definition.nodes_.end(),
                         [&](const auto& node) { return node.id_ == definition.output_; });
    const auto descriptor = registry.Find(output->type_, document.components_);
    for (const auto& property : descriptor->properties_)
        definition.parameters_.push_back(
                {property.key_, output->id_, property.key_,
                 definition.title_.size() <= 128 ? definition.title_ : std::string{}});
    next.document_.components_.push_back(definition);
    if (!registry.Find(definition.type_, next.document_.components_))
        return graph::Diagnostic{"graph.component"};
    std::erase_if(next.document_.nodes_,
                  [&](const auto& node) { return selected.contains(node.id_); });
    next.document_.nodes_.push_back(
            registry.MakeNode(instance, definition.type_, next.document_.components_));
    if (selected.contains(document.output_)) next.document_.output_ = instance;
    const auto position = snapshot.positions_.contains(definition.output_)
                                  ? snapshot.positions_.at(definition.output_)
                                  : Position{};
    for (const auto id : selected) {
        if (const auto found = next.positions_.find(id); found != next.positions_.end())
            next.component_positions_[definition.type_][id] = found->second;
        next.positions_.erase(id);
    }
    next.positions_[instance] = position;
    return next;
}
EditResult ExpandAllComponents(const Snapshot& snapshot, const graph::Registry& registry,
                               graph::NodeId first_fresh_id) {
    if (snapshot.document_.components_.empty()) return snapshot;
    auto expanded = graph::ExpandComponents(snapshot.document_, registry, first_fresh_id);
    if (std::holds_alternative<std::vector<graph::Diagnostic>>(expanded))
        return std::get<std::vector<graph::Diagnostic>>(expanded).front();
    auto next = snapshot;
    next.document_ = std::get<graph::Document>(std::move(expanded));
    next.document_.extensions_ = snapshot.document_.extensions_;
    next.component_positions_.clear();
    std::size_t index = 0;
    for (const auto& node : next.document_.nodes_) {
        if (!next.positions_.contains(node.id_)) {
            next.positions_[node.id_] = {40.0F + static_cast<float>(index % 4) * 300,
                                         400.0F + static_cast<float>(index / 4) * 220};
            ++index;
        }
    }
    return next;
}
}  // namespace rhythm::editor
