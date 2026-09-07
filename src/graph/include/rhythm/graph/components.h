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
}  // namespace rhythm::graph
