#include <algorithm>
#include <array>
#include <limits>
#include <set>

#include "rhythm/editor/commands.h"
#include "rhythm/graph/components.h"

namespace rhythm::editor {
EditResult UnpackComponent(const Snapshot& snapshot, const graph::Registry& registry,
                           graph::NodeId instance, graph::NodeId first_fresh_id) {
    // Reuse the normal expander to validate interfaces and the concrete instance
    // before moving its immediate body into the parent authoring scope.
    const auto validation = graph::ExpandComponentScope(snapshot.document_, registry,
                                                        std::array<graph::NodeId, 1>{instance});
    if (std::holds_alternative<std::vector<graph::Diagnostic>>(validation))
        return std::get<std::vector<graph::Diagnostic>>(validation).front();
    const auto& document = snapshot.document_;
    const auto selected = std::find_if(document.nodes_.begin(), document.nodes_.end(),
                                       [&](const auto& node) { return node.id_ == instance; });
    const auto definition =
            std::find_if(document.components_.begin(), document.components_.end(),
                         [&](const auto& item) { return item.type_ == selected->type_; });
    auto next_node = first_fresh_id ? first_fresh_id - 1 : 0;
    for (const auto& node : document.nodes_) next_node = std::max(next_node, node.id_);
    std::map<graph::NodeId, graph::NodeId> ids;
    for (const auto& node : definition->nodes_) {
        if (node.id_ == definition->output_) {
            ids[node.id_] = instance;
        } else {
            if (next_node == std::numeric_limits<graph::NodeId>::max())
                return graph::Diagnostic{"graph.limit"};
            ids[node.id_] = ++next_node;
        }
    }
    auto next = snapshot;
    auto body = definition->nodes_;
    // Same exposed-property substitution as component_expansion.cpp. Nested
    // component nodes keep their substituted instance properties at this level.
    for (const auto& parameter : definition->parameters_) {
        const auto value = selected->properties_.find(parameter.key_);
        if (value == selected->properties_.end()) continue;
        const auto target = std::find_if(body.begin(), body.end(), [&](const auto& node) {
            return node.id_ == parameter.node_;
        });
        target->properties_[parameter.property_] = value->second;
    }
    std::erase_if(next.document_.nodes_, [&](const auto& node) { return node.id_ == instance; });
    for (auto node : body) {
        node.id_ = ids.at(node.id_);
        next.document_.nodes_.push_back(std::move(node));
    }
    std::set<std::pair<graph::NodeId, std::string>> overridden;
    const auto remap_input = [&](graph::NodeId& node, std::string& input) {
        if (node != instance) return;
        const auto port = std::find_if(definition->inputs_.begin(), definition->inputs_.end(),
                                       [&](const auto& value) { return value.key_ == input; });
        overridden.emplace(port->node_, port->input_);
        node = ids.at(port->node_);
        input = port->input_;
    };
    for (auto& edge : next.document_.edges_) remap_input(edge.to_, edge.input_);
    for (auto& binding : next.document_.bindings_) remap_input(binding.node_, binding.input_);
    std::uint64_t next_edge = 0;
    for (const auto& edge : document.edges_) next_edge = std::max(next_edge, edge.id_);
    for (auto edge : definition->edges_) {
        if (overridden.contains({edge.to_, edge.input_})) continue;
        if (next_edge == std::numeric_limits<std::uint64_t>::max())
            return graph::Diagnostic{"graph.limit"};
        edge.id_ = ++next_edge;
        edge.from_ = ids.at(edge.from_);
        edge.to_ = ids.at(edge.to_);
        next.document_.edges_.push_back(std::move(edge));
    }
    std::set<std::string> names;
    for (const auto& signal : document.signals_) names.insert(signal.name_);
    std::map<std::string, std::string> signals;
    std::size_t suffix = 0;
    for (auto signal : definition->signals_) {
        const auto previous = signal.name_;
        do {
            signal.name_ =
                    "component." + std::to_string(instance) + ".signal." + std::to_string(++suffix);
        } while (names.contains(signal.name_));
        names.insert(signal.name_);
        signals[previous] = signal.name_;
        signal.source_ = ids.at(signal.source_);
        next.document_.signals_.push_back(std::move(signal));
    }
    for (auto binding : definition->bindings_) {
        if (overridden.contains({binding.node_, binding.input_})) continue;
        binding.node_ = ids.at(binding.node_);
        binding.signal_ = signals.at(binding.signal_);
        next.document_.bindings_.push_back(std::move(binding));
    }
    const auto origin =
            snapshot.positions_.contains(instance) ? snapshot.positions_.at(instance) : Position{};
    const auto found_layout = snapshot.component_positions_.find(definition->type_);
    const std::map<graph::NodeId, Position> empty_layout;
    const auto& layout = found_layout == snapshot.component_positions_.end() ? empty_layout
                                                                             : found_layout->second;
    const auto anchor =
            layout.contains(definition->output_) ? layout.at(definition->output_) : Position{};
    std::size_t index = 0;
    for (const auto& node : body) {
        if (node.id_ == definition->output_) {
            next.positions_[instance] = origin;
        } else if (const auto position = layout.find(node.id_); position != layout.end()) {
            next.positions_[ids.at(node.id_)] = {origin.x_ + position->second.x_ - anchor.x_,
                                                 origin.y_ + position->second.y_ - anchor.y_};
        } else {
            next.positions_[ids.at(node.id_)] = {
                    origin.x_ + static_cast<float>(index % 4) * 300,
                    origin.y_ + 260 + static_cast<float>(index / 4) * 220};
            ++index;
        }
    }
    const auto expanded = graph::ExpandComponents(next.document_, registry);
    if (std::holds_alternative<std::vector<graph::Diagnostic>>(expanded))
        return std::get<std::vector<graph::Diagnostic>>(expanded).front();
    return next;
}
}  // namespace rhythm::editor
