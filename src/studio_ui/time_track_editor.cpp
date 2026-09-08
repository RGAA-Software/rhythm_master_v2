#include "time_track_editor.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>

#include "curve_editor.h"
#include "rhythm/editor/commands.h"

namespace rhythm::studio {
void TimeTrackEditor::Reset() {
    draft_.reset();
    sections_.Reset();
    curve_editor_.Reset();
    curve_draft_ = false;
    section_error_.clear();
}
TimelineEdit TimeTrackEditor::Draw(const editor::Snapshot& base, double playhead, double duration,
                                   const std::map<std::string, std::string>& text,
                                   const std::function<graph::NodeId()>& reserve_id,
                                   const std::string& section_action) {
    TimelineEdit edit;
    if (draft_ && (draft_->document_.revision_ != base.document_.revision_ ||
                   draft_->document_.id_ != base.document_.id_)) {
        Reset();
        edit.preview_changed_ = true;
    }
    ImGui::Separator();
    ImGui::BeginDisabled(draft_ && curve_draft_);
    const auto section =
            sections_.Draw((draft_ ? *draft_ : base).document_, duration,
                           static_cast<bool>(reserve_id) && !draft_, text, section_action);
    ImGui::EndDisabled();
    if (section.add_) {
        const auto section_id = reserve_id();
        const auto clock_id = reserve_id();
        auto result = editor::AddTimeSection(
                base, graph::Registry{}, std::clamp(playhead, 0.0, 86400.0), section_id, clock_id);
        if (std::holds_alternative<editor::Snapshot>(result)) {
            edit.committed_ = std::move(std::get<editor::Snapshot>(result));
            section_error_.clear();
        } else {
            section_error_ = std::get<graph::Diagnostic>(result).code_;
        }
        return edit;
    }
    if (!section_error_.empty()) ImGui::TextWrapped("%s", text.at(section_error_).c_str());
    if (section.changed_) {
        if (!draft_) draft_ = base;
        for (auto& node : draft_->document_.nodes_)
            if (node.id_ == section.changed_->id_) node = *section.changed_;
        curve_draft_ = false;
        edit.preview_changed_ = true;
    }
    if (section.committed_ && draft_ && !curve_draft_) {
        edit.committed_ = std::move(draft_);
        Reset();
        return edit;
    }
    // Finish the active section transaction before entering a curve editor.
    if (draft_ && !curve_draft_) return edit;
    ImGui::Separator();
    const auto& snapshot = draft_ ? *draft_ : base;
    std::vector<graph::NodeId> tracks;
    for (const auto& node : snapshot.document_.nodes_)
        if (node.type_ == "scalar.curve") tracks.push_back(node.id_);
    if (tracks.empty()) {
        ImGui::TextWrapped("%s", text.at("timeline.no_tracks").c_str());
        return edit;
    }
    if (std::find(tracks.begin(), tracks.end(), track_) == tracks.end()) track_ = tracks.front();
    const auto title = [&](graph::NodeId id) {
        return text.at("scalar.curve") + " #" + std::to_string(id);
    };
    if (ImGui::BeginCombo(text.at("timeline.track").c_str(), title(track_).c_str())) {
        for (const auto id : tracks)
            if (ImGui::Selectable(title(id).c_str(), id == track_)) {
                track_ = id;
                if (draft_) {
                    edit.committed_ = std::move(draft_);
                    draft_.reset();
                }
            }
        ImGui::EndCombo();
    }
    if (edit.committed_) return edit;
    ImGui::TextWrapped("%s", text.at("timeline.curve_help").c_str());
    const auto found =
            std::find_if(snapshot.document_.nodes_.begin(), snapshot.document_.nodes_.end(),
                         [&](const auto& node) { return node.id_ == track_; });
    if (found->properties_.contains("curve") &&
        !std::holds_alternative<parameters::Curve>(found->properties_.at("curve"))) {
        ImGui::TextWrapped("%s", text.at("invalid_curve").c_str());
        return edit;
    }
    auto curve = found->properties_.contains("curve")
                         ? std::get<parameters::Curve>(found->properties_.at("curve"))
                         : parameters::Curve{};
    const auto curve_edit =
            curve_editor_.Draw(curve, "timeline.curve." + std::to_string(track_), text);
    if (curve_edit.changed_) {
        if (!draft_) draft_ = base;
        curve_draft_ = true;
        for (auto& node : draft_->document_.nodes_)
            if (node.id_ == track_) node.properties_["curve"] = std::move(curve);
        edit.preview_changed_ = true;
    }
    if (curve_edit.committed_ && draft_) {
        edit.committed_ = std::move(draft_);
        draft_.reset();
    }
    return edit;
}
}  // namespace rhythm::studio
