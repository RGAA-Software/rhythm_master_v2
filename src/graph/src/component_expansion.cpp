#include <algorithm>
#include <limits>
#include <map>
#include <set>

#include "rhythm/graph/bindings.h"
#include "rhythm/graph/components.h"

namespace rhythm::graph {
namespace {
struct Failure {
    Diagnostic diagnostic_{};
};
void Require(bool valid, NodeId owner = 0) {
    if (!valid) throw Failure{{"graph.component", owner}};
}
struct Incoming {
    NodeId source_ = 0;
    NodeId target_ = 0;
    std::string input_{};
};
struct Expansion {
    Document result_{};
    NodeId next_node_ = 0;
    std::vector<std::string> active_{};
    std::vector<NodeId> path_{};
    std::vector<NodeId> requested_path_{};
    std::map<NodeId, NodeId> scope_nodes_{};
    std::map<NodeId, AuthorNode> authors_{};
    bool scope_found_ = false;
};
NodeId Allocate(Expansion& state) {
    Require(state.next_node_ != std::numeric_limits<NodeId>::max());
    return ++state.next_node_;
}
void ExpandScope(const Document& body, const std::map<NodeId, NodeId>& ids,
                 std::span<const Incoming> external,
                 std::span<const ComponentDefinition> definitions, const Registry& registry,
                 Expansion& state) {
    if (state.path_ == state.requested_path_) {
        state.scope_nodes_ = ids;
        state.scope_found_ = true;
    }
    const auto resolved = ResolveEdges(body);
    if (std::holds_alternative<std::vector<Diagnostic>>(resolved))
        throw Failure{std::get<std::vector<Diagnostic>>(resolved).front()};
    std::map<NodeId, std::vector<Incoming>> incoming;
    std::set<std::uint64_t> edge_ids;
    for (const auto& edge : std::get<std::vector<Edge>>(resolved)) {
        Require(edge.id_ && edge_ids.insert(edge.id_).second && ids.contains(edge.from_) &&
                ids.contains(edge.to_));
        const bool replaced = std::any_of(external.begin(), external.end(), [&](const auto& value) {
            return value.target_ == edge.to_ && value.input_ == edge.input_;
        });
        if (!replaced) incoming[edge.to_].push_back({ids.at(edge.from_), edge.to_, edge.input_});
    }
    for (const auto& edge : external) {
        Require(ids.contains(edge.target_));
        incoming[edge.target_].push_back(edge);
    }
    for (const auto& node : body.nodes_) {
        const auto id = ids.at(node.id_);
        const auto descriptor = registry.Find(node.type_, definitions);
        Require(descriptor.has_value(), id);
        const auto validation = registry.ValidateNode(node, definitions);
        if (!validation.empty()) throw Failure{validation.front()};
        if (descriptor->operation_ != Operation::kComponent) {
            Require(state.result_.nodes_.size() < 10000, id);
            Require(state.authors_.emplace(id, AuthorNode{state.path_, node.id_}).second, id);
            auto expanded = node;
            expanded.id_ = id;
            state.result_.nodes_.push_back(std::move(expanded));
            for (const auto& edge : incoming[node.id_]) {
                Require(state.result_.edges_.size() < 40000, id);
                state.result_.edges_.push_back(
                        {state.result_.edges_.size() + 1, edge.source_, id, edge.input_});
            }
            continue;
        }
        Require(state.active_.size() < 16 && std::find(state.active_.begin(), state.active_.end(),
                                                       node.type_) == state.active_.end(),
                id);
        const auto prototype =
                std::find_if(definitions.begin(), definitions.end(),
                             [&](const auto& value) { return value.type_ == node.type_; });
        Require(prototype != definitions.end(), id);
        Document inner;
        inner.nodes_ = prototype->nodes_;
        inner.edges_ = prototype->edges_;
        inner.signals_ = prototype->signals_;
        inner.bindings_ = prototype->bindings_;
        for (const auto& parameter : prototype->parameters_) {
            const auto value = node.properties_.find(parameter.key_);
            if (value == node.properties_.end()) continue;
            const auto target =
                    std::find_if(inner.nodes_.begin(), inner.nodes_.end(),
                                 [&](const auto& item) { return item.id_ == parameter.node_; });
            Require(target != inner.nodes_.end(), id);
            target->properties_[parameter.property_] = value->second;
        }
        std::map<NodeId, NodeId> inner_ids;
        for (const auto& child : inner.nodes_) {
            Require(child.id_ && !inner_ids.contains(child.id_), id);
            inner_ids[child.id_] = child.id_ == prototype->output_ ? id : Allocate(state);
        }
        std::vector<Incoming> mapped;
        for (const auto& edge : incoming[node.id_]) {
            const auto port =
                    std::find_if(prototype->inputs_.begin(), prototype->inputs_.end(),
                                 [&](const auto& value) { return value.key_ == edge.input_; });
            Require(port != prototype->inputs_.end(), id);
            mapped.push_back({edge.source_, port->node_, port->input_});
        }
        state.active_.push_back(node.type_);
        state.path_.push_back(node.id_);
        ExpandScope(inner, inner_ids, mapped, definitions, registry, state);
        state.path_.pop_back();
        state.active_.pop_back();
    }
}
}  // namespace
ComponentScopeExpansion ExpandComponentScope(const Document& document, const Registry& registry,
                                             std::span<const NodeId> instance_path,
                                             NodeId minimum_generated_id) {
    try {
        Require(document.components_.size() <= 256 && document.nodes_.size() <= 10000);
        std::set<std::string> types;
        std::size_t stored_nodes = document.nodes_.size();
        std::size_t stored_edges = document.edges_.size();
        for (const auto& definition : document.components_) {
            stored_nodes += definition.nodes_.size();
            stored_edges += definition.edges_.size() + definition.bindings_.size();
            Require(stored_nodes <= 10000 && stored_edges <= 40000 &&
                    definition.type_.size() <= 256 && definition.type_.starts_with("component.") &&
                    types.insert(definition.type_).second);
            Require(DescribeComponent(definition.type_, registry, document.components_)
                            .has_value());
        }
        Expansion state;
        Require(instance_path.size() <= 16);
        state.requested_path_.assign(instance_path.begin(), instance_path.end());
        state.next_node_ = minimum_generated_id ? minimum_generated_id - 1 : 0;
        state.result_.id_ = document.id_;
        state.result_.revision_ = document.revision_;
        state.result_.output_ = document.output_;
        state.result_.canvas_ = document.canvas_;
        state.result_.control_titles_ = document.control_titles_;
        state.result_.control_snapshots_ = document.control_snapshots_;
        state.result_.control_cues_ = document.control_cues_;
        state.result_.beat_grid_ = document.beat_grid_;
        std::map<NodeId, NodeId> ids;
        for (const auto& node : document.nodes_) {
            Require(node.id_ && ids.emplace(node.id_, node.id_).second, node.id_);
            state.next_node_ = std::max(state.next_node_, node.id_);
        }
        ExpandScope(document, ids, {}, document.components_, registry, state);
        if (!state.scope_found_) return std::vector<Diagnostic>{{"graph.missing_viewer"}};
        return ExpandedComponentScope{std::move(state.result_), std::move(state.scope_nodes_),
                                      std::move(state.authors_)};
    } catch (const Failure& failure) {
        return std::vector<Diagnostic>{failure.diagnostic_};
    }
}
ComponentExpansion ExpandComponents(const Document& document, const Registry& registry,
                                    NodeId minimum_generated_id) {
    auto expanded = ExpandComponentScope(document, registry, {}, minimum_generated_id);
    if (std::holds_alternative<std::vector<Diagnostic>>(expanded))
        return std::get<std::vector<Diagnostic>>(std::move(expanded));
    return std::get<ExpandedComponentScope>(std::move(expanded)).document_;
}
}  // namespace rhythm::graph
