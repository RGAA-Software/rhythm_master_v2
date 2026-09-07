#include "timeline_panel.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>

#include "curve_editor.h"

namespace rhythm::studio {
void TimelinePanel::Restart() {
    clock_.Seek(0);
    ++generation_;
}
double TimelinePanel::Advance(double host_seconds, bool seekable) {
    const auto seconds = clock_.Advance(host_seconds, paused_);
    if (loop_ && seekable && seconds >= duration_) {
        clock_.Seek(std::fmod(seconds, duration_));
        ++generation_;
    }
    return clock_.Seconds();
}
TimelineEdit TimelinePanel::Draw(const editor::Snapshot& base, bool seekable,
                                 const std::map<std::string, std::string>& text) {
    TimelineEdit edit;
    if (draft_ && (draft_->document_.revision_ != base.document_.revision_ ||
                   draft_->document_.id_ != base.document_.id_)) {
        draft_.reset();
        edit.preview_changed_ = true;
    }
    if (ImGui::Button(text.at(paused_ ? "timeline.play" : "timeline.pause").c_str()))
        paused_ = !paused_;
    ImGui::SameLine();
    if (ImGui::Button(text.at("timeline.restart").c_str())) Restart();
    ImGui::SameLine();
    ImGui::BeginDisabled(!seekable);
    ImGui::Checkbox(text.at("timeline.loop").c_str(), &loop_);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(110);
    const std::array labels{text.at("timeline.seconds"), text.at("timeline.frames"),
                            text.at("timeline.beats")};
    if (ImGui::BeginCombo("###timeline.unit", labels[unit_].c_str())) {
        for (int index = 0; index < 3; ++index)
            if (ImGui::Selectable(labels[index].c_str(), unit_ == index)) unit_ = index;
        ImGui::EndCombo();
    }
    if (unit_ > 0) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120);
        const double minimum = 1, maximum = unit_ == 1 ? 240 : 400;
        auto& rate = unit_ == 1 ? fps_ : bpm_;
        ImGui::DragScalar(unit_ == 1 ? "FPS" : "BPM", ImGuiDataType_Double, &rate, 1, &minimum,
                          &maximum, "%.1f", ImGuiSliderFlags_AlwaysClamp);
        rate = std::isfinite(rate) ? std::clamp(rate, minimum, maximum) : minimum;
    }
    const auto factor = unit_ == 0 ? 1.0 : unit_ == 1 ? fps_ : bpm_ / 60;
    auto position = clock_.Seconds() * factor;
    const double minimum = 0, maximum = duration_ * factor;
    ImGui::BeginDisabled(!seekable);
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderScalar("###timeline.position", ImGuiDataType_Double, &position, &minimum,
                            &maximum, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
        if (std::isfinite(position)) {
            if (unit_ == 1) position = std::round(position);
            clock_.Seek(std::clamp(position / factor, 0.0, duration_));
            ++generation_;
        }
    }
    ImGui::EndDisabled();
    ImGui::SetNextItemWidth(150);
    const double duration_minimum = 0.01, duration_maximum = 86400;
    ImGui::DragScalar(text.at("timeline.duration").c_str(), ImGuiDataType_Double, &duration_, 0.1f,
                      &duration_minimum, &duration_maximum, "%.2f s", ImGuiSliderFlags_AlwaysClamp);
    duration_ = std::isfinite(duration_) ? std::clamp(duration_, duration_minimum, duration_maximum)
                                         : 10;
    if (!seekable) ImGui::TextWrapped("%s", text.at("timeline.stateful").c_str());
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
            DrawCurveEditor(curve, "timeline.curve." + std::to_string(track_), text);
    if (curve_edit.changed_) {
        if (!draft_) draft_ = base;
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
