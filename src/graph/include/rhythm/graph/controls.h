#pragma once

#include "rhythm/graph/compiler.h"

namespace rhythm::graph {
// Build and validate the authoring control bank, including disconnected nodes.
// Component inputs can receive root macros through ordinary typed scalar edges.
parameters::ControlBank DescribeControls(const Document& document);
// Remove metadata for deleted control nodes, preserving surviving snapshot IDs.
void PruneControls(Document& document);
// Publishing drops disconnected controls and their snapshot entries while the
// editable document retains them. Runtime metadata refers only to plan IDs.
parameters::ControlBank SelectControls(const parameters::ControlBank& bank,
                                       std::span<const Instruction> instructions);
}  // namespace rhythm::graph
