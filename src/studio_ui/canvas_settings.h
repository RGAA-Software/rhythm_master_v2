#pragma once

#include <map>
#include <optional>

#include "rhythm/graph/document.h"

namespace rhythm::studio {
std::optional<graph::Canvas> DrawCanvasSettings(graph::Canvas canvas,
                                                const std::map<std::string, std::string>& text);
}  // namespace rhythm::studio
