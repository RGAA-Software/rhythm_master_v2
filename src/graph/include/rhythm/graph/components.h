#pragma once

#include "rhythm/graph/registry.h"

namespace rhythm::graph {
std::optional<OperatorDescriptor> DescribeComponent(
        std::string_view type, const Registry& registry,
        std::span<const ComponentDefinition> definitions);
using ComponentExpansion = std::variant<Document, std::vector<Diagnostic>>;
// Bounded deterministic expansion, retaining each instance ID at its output.
// Reuses ordinary edges/properties; backend and component types never enter the
// expanded execution plan. Maximum nested depth is 16, including indirect cycles.
ComponentExpansion ExpandComponents(const Document& document, const Registry& registry,
                                    NodeId minimum_generated_id = 0);
struct AuthorNode {
    // Root instance ID followed by nested local instance IDs; empty for root.
    std::vector<NodeId> instance_path_{};
    NodeId node_ = 0;
    bool operator==(const AuthorNode&) const = default;
};
struct ExpandedComponentScope {
    Document document_{};
    // Local body IDs -> expanded executable IDs for one concrete instance path.
    // An empty path addresses root nodes; nested path elements are local IDs.
    std::map<NodeId, NodeId> nodes_{};
    // Every expanded executable ID -> its exact authored definition/node path.
    // Even a preserved root instance output ID can address a nested author node.
    // Metadata only: never serialized into runtime packages or graph properties.
    std::map<NodeId, AuthorNode> authors_{};
};
using ComponentScopeExpansion = std::variant<ExpandedComponentScope, std::vector<Diagnostic>>;
ComponentScopeExpansion ExpandComponentScope(const Document& document, const Registry& registry,
                                             std::span<const NodeId> instance_path,
                                             NodeId minimum_generated_id = 0);
}  // namespace rhythm::graph
