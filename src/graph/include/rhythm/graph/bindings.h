#pragma once

#include <string_view>

#include "rhythm/graph/document.h"

namespace rhythm::graph {
bool ValidSignalName(std::string_view name);
// Graph-scope names resolve to ordinary dependencies; Compile validates types
// and cycles. Authoring data is unchanged and no unresolved names reach Player.
using EdgeResolution = std::variant<std::vector<Edge>, std::vector<Diagnostic>>;
EdgeResolution ResolveEdges(const Document& document);
}  // namespace rhythm::graph
