#pragma once

#include "binding_editor.h"
#include "expression_editor.h"
#include "rhythm/content/presets.h"
#include "rhythm/editor/history.h"

namespace rhythm::studio {
struct InspectorResult {
    std::optional<editor::Snapshot> committed_{};
    bool preview_changed_ = false;
    std::optional<graph::Diagnostic> diagnostic_{};
};
// Owns the lifetime of one parameter-edit transaction and its live preview.
// All mutation and drawing stay on the UI thread; commits remain value snapshots.
class PropertyInspector final {
   public:
    InspectorResult Draw(const editor::Snapshot& base, graph::NodeId selected,
                         const graph::Registry& registry, std::span<const content::Preset> presets,
                         const std::map<std::string, std::string>& text, const std::string& locale);
    const std::optional<editor::Snapshot>& Preview() const { return draft_; }
    void Reset() {
        draft_.reset();
        expression_editor_.Reset();
        binding_editor_.Reset();
    }

   private:
    std::optional<editor::Snapshot> draft_{};
    ExpressionEditor expression_editor_{};
    BindingEditor binding_editor_{};
};
}  // namespace rhythm::studio
