#pragma once

#include "rhythm/graph/document.h"

namespace rhythm::project::detail {
// Structural persistence limits; compiler separately validates execution types
// and cycles so incomplete/unknown native operators can still be retained.
void ValidateGraph(const graph::Document& document);
}  // namespace rhythm::project::detail
