#pragma once

#include <imgui.h>

#include <map>
#include <optional>

#include "rhythm/graph/registry.h"

namespace rhythm::studio {
// Owns filtering and category navigation for both graph and component editors.
// Selection returns a stable type; document edits remain with the caller.
class NodePalette final {
   public:
    std::optional<std::string> Draw(std::span<const graph::OperatorDescriptor> entries,
                                    const std::map<std::string, std::string>& text,
                                    const std::string& popup_id);

   private:
    ImGuiTextFilter filter_{};
};
}  // namespace rhythm::studio
