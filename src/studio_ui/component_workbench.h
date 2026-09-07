#pragma once

#include "component_interface.h"
#include "graph_canvas.h"
#include "node_palette.h"
#include "property_inspector.h"
#include "rhythm/editor/component_edit.h"

namespace rhythm::studio {
// Owns nested authoring navigation and its draft lifecycle; application history
// receives one validated value only when the user applies the draft.
class ComponentWorkbench final {
   public:
    void Open(const editor::Snapshot& project, std::string type);
    std::optional<editor::Snapshot> Draw(const editor::Snapshot& project,
                                         const graph::Registry& registry,
                                         const std::map<std::string, std::string>& text,
                                         const std::string& locale);

   private:
    void CommitPreview();
    void ResetView();
    std::optional<editor::ComponentEdit> edit_{};
    GraphCanvas canvas_{};
    NodePalette node_palette_{};
    PropertyInspector inspector_{};
    ComponentInterface interface_{};
    std::string status_{};
};
}  // namespace rhythm::studio
