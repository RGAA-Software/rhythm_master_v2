#include "timeline_panel.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

#include "curve_editor.h"
#include "rhythm/editor/commands.h"

namespace rhythm::studio {
std::size_t TimelinePanel::WaveformBins() const {
#ifdef RHYTHM_HAS_LOCAL_MEDIA
    return waveform_.BinCount();
#else
    return 0;
#endif
}
void TimelinePanel::CancelMediaPreview() {
#ifdef RHYTHM_HAS_LOCAL_MEDIA
    waveform_.Clear();
#endif
}
void TimelinePanel::Restart() {
    clock_.Seek(0);
    command_.seek_ = 0;
}
runtime::PlaybackCommand TimelinePanel::TakePlaybackCommand() {
    return std::exchange(command_, {});
}
double TimelinePanel::Advance(double host_seconds, bool,
                              const std::optional<runtime::PlaybackSample>& source) {
    const auto seconds = clock_.Advance(host_seconds, false, source);
    if (source && source->duration_) duration_ = *source->duration_;
    if (loop_ && !source && seconds >= duration_) {
        clock_.Seek(std::fmod(seconds, duration_));
    }
    return clock_.Seconds();
}
TimelineEdit TimelinePanel::Draw(const editor::Snapshot& base, bool seekable,
                                 const std::map<std::string, std::string>& text,
                                 const std::optional<std::filesystem::path>& music,
                                 const std::function<graph::NodeId()>& reserve_id) {
    TimelineEdit edit;
    if (draft_ && (draft_->document_.revision_ != base.document_.revision_ ||
                   draft_->document_.id_ != base.document_.id_)) {
        ResetEdit();
        edit.preview_changed_ = true;
    }
    if (ImGui::Button(text.at(Paused() ? "timeline.play" : "timeline.pause").c_str())) {
        clock_.SetPaused(!Paused());
        command_.paused_ = Paused();
    }
    ImGui::SameLine();
    if (ImGui::Button(text.at("timeline.restart").c_str())) Restart();
    ImGui::SameLine();
    ImGui::BeginDisabled(clock_.FollowingMedia());
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
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderScalar("###timeline.position", ImGuiDataType_Double, &position, &minimum,
                            &maximum, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
        if (std::isfinite(position)) {
            if (unit_ == 1) position = std::round(position);
            clock_.Seek(std::clamp(position / factor, 0.0, duration_));
            command_.seek_ = clock_.Seconds();
        }
    }
    ImGui::SetNextItemWidth(150);
    ImGui::BeginDisabled(clock_.FollowingMedia());
    const double duration_minimum = 0.01, duration_maximum = 86400;
    ImGui::DragScalar(text.at("timeline.duration").c_str(), ImGuiDataType_Double, &duration_, 0.1f,
                      &duration_minimum, &duration_maximum, "%.2f s", ImGuiSliderFlags_AlwaysClamp);
    duration_ = std::isfinite(duration_) ? std::clamp(duration_, duration_minimum, duration_maximum)
                                         : 10;
    ImGui::EndDisabled();
    if (clock_.FollowingMedia()) ImGui::TextWrapped("%s", text.at("timeline.media_clock").c_str());
    if (!seekable) ImGui::TextWrapped("%s", text.at("timeline.stateful").c_str());
#ifdef RHYTHM_HAS_LOCAL_MEDIA
    if (const auto seek = waveform_.Draw(music, clock_.Seconds(), text)) {
        clock_.Seek(*seek);
        command_.seek_ = *seek;
    }
#else
    (void)music;
#endif
    ImGui::Separator();
    ImGui::BeginDisabled(draft_ && curve_draft_);
    const auto section = sections_.Draw((draft_ ? *draft_ : base).document_, duration_,
                                        static_cast<bool>(reserve_id) && !draft_, text);
    ImGui::EndDisabled();
    if (section.add_) {
        const auto section_id = reserve_id();
        const auto clock_id = reserve_id();
        auto result = editor::AddTimeSection(base, graph::Registry{},
                                             std::clamp(clock_.Seconds(), 0.0, 86400.0), section_id,
                                             clock_id);
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
        ResetEdit();
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
            DrawCurveEditor(curve, "timeline.curve." + std::to_string(track_), text);
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
