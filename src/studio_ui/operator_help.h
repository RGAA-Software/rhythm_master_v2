#pragma once

#include "rhythm/graph/registry.h"

namespace rhythm::studio {
// Localized help derived from the same contracts used to validate graph connections.
void DrawOperatorHelp(const graph::OperatorDescriptor& descriptor,
                      const std::map<std::string, std::string>& text, bool properties = true);
}  // namespace rhythm::studio
