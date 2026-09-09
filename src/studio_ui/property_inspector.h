#pragma once
#include "binding_editor.h"
#include "curve_editor.h"
#include "event_track_editor.h"
#include "expression_editor.h"
#include "rhythm/assets/images.h"
#include "rhythm/content/presets.h"
#include "rhythm/control_ui/control_panel.h"
#include "rhythm/editor/history.h"
#include "rhythm/scene/resources.h"
#include "text_editor.h"

namespace rhythm::studio {
struct InspectorResult {
    std::optional<editor::Snapshot> committed_{};
    bool preview_changed_ = false;
    std::optional<graph::Diagnostic> diagnostic_{};
    std::optional<std::uint64_t> recall_{};
    bool follow_cues_ = false;
};
// Owns the lifetime of one parameter-edit transaction and its live preview.
// All mutation and drawing stay on the UI thread; commits remain value snapshots.
class PropertyInspector final {
   public:
    InspectorResult Draw(const editor::Snapshot& base, graph::NodeId selected,
                         const graph::Registry& registry, std::span<const content::Preset> presets,
                         const std::map<std::string, std::string>& text, const std::string& locale,
                         const scene::Resources& models = {}, double seconds = 0,
                         bool defer_recall = false, const assets::Images& images = {});
    parameters::ControlValues LiveControls(const parameters::ControlBank& bank) const;
    void PerformControls(parameters::ControlValues values);
    const std::optional<editor::Snapshot>& Preview() const { return draft_; }
    void Reset() {
        draft_.reset();
        expression_editor_.Reset();
        text_editor_.Reset();
        binding_editor_.Reset();
        curve_editor_.Reset();
        event_track_editor_.Reset();
        controls_.Reset();
        live_controls_.clear();
        performance_controls_.clear();
        control_sequence_.reset();
        sequence_bank_ = {};
        sequence_cues_.clear();
    }

   private:
    bool DrawControls(const editor::Snapshot& snapshot,
                      const std::map<std::string, std::string>& text, InspectorResult& result,
                      double seconds, bool defer_recall);
    std::optional<editor::Snapshot> draft_{};
    ExpressionEditor expression_editor_{};
    TextEditor text_editor_{};
    BindingEditor binding_editor_{};
    CurveEditor curve_editor_{};
    EventTrackEditor event_track_editor_{};
    control_ui::ControlPanel controls_{};
    parameters::ControlValues live_controls_{};
    parameters::ControlValues performance_controls_{};
    parameters::ControlBank sequence_bank_{};
    std::vector<parameters::ControlCue> sequence_cues_{};
    std::optional<parameters::ControlSequence> control_sequence_{};
};
}  // namespace rhythm::studio
