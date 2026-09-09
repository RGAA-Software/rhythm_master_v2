#pragma once

#include "rhythm/graph/registry.h"

namespace rhythm::studio {
// Disambiguate shared property/port keys without changing stable graph or UI IDs.
std::string OperatorFieldKey(const std::string& type, const std::string& key);
// Localized help derived from the same contracts used to validate graph connections.
void DrawOperatorHelp(const graph::OperatorDescriptor& descriptor,
                      const std::map<std::string, std::string>& text, bool properties = true);
}  // namespace rhythm::studio
