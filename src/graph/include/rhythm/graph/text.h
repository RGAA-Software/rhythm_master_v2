#pragma once

#include "rhythm/graph/document.h"

namespace rhythm::graph {
bool ValidText(std::string_view text);
// Exact, versioned layout identity within a font asset. Changes in any authored
// layout property invalidate prepared masks without depending on graph/node IDs.
std::string TextImageKey(const Node& node);
}  // namespace rhythm::graph
