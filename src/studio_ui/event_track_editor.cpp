#include "event_track_editor.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <limits>

namespace rhythm::studio {
void EventTrackEditor::Reset() {
    previous_.reset();
    selected_.reset();
    error_.clear();
}
std::optional<parameters::EventTrack> EventTrackEditor::Draw(
        const parameters::EventTrack& track, const std::string& id, double seconds,
        const std::map<std::string, std::string>& text) {
    if (id != id_ || (previous_ && *previous_ != track)) Reset();
    id_ = id;
    previous_ = track;
    ImGui::PushID(id.c_str());
    const auto label = [&](const std::string& key) { return text.at(key) + "###" + key; };
    const std::array kinds{text.at("event.pulse"), text.at("event.gate_kind"),
                           text.at("event.reset_action")};
    ImGui::Text("%s: %zu / %zu", text.at("actions").c_str(), track.Events().size(),
                parameters::EventTrack::kMaximumEvents);
    const auto origin = ImGui::GetCursorScreenPos();
    const auto width = std::max(80.0f, ImGui::GetContentRegionAvail().x);
    const double end =
            std::max({10.0, seconds, track.Events().empty() ? 0 : track.Events().back().seconds_});
    ImGui::InvisibleButton("track_plot", {width, 64});
    // Synchronous ImGui drawing borrow, never retained across calls.
    auto& draw = *ImGui::GetWindowDrawList();
    draw.AddRectFilled(origin, {origin.x + width, origin.y + 64}, IM_COL32(20, 28, 38, 255));
    float nearest = 9;
    for (const auto& action : track.Events()) {
        const auto x = origin.x + 4 + static_cast<float>(action.seconds_ / end) * (width - 8);
        const bool active = selected_ && selected_->id_ == action.id_;
        draw.AddLine({x, origin.y + 12}, {x, origin.y + 50},
                     active ? IM_COL32(255, 255, 255, 255) : IM_COL32(255, 177, 60, 255),
                     active ? 2.0f : 1.0f);
        if (ImGui::IsItemClicked() && std::abs(ImGui::GetIO().MousePos.x - x) < nearest) {
            nearest = std::abs(ImGui::GetIO().MousePos.x - x);
            selected_ = action;
        }
    }
    const auto playhead = origin.x + 4 + static_cast<float>(seconds / end) * (width - 8);
    draw.AddLine({playhead, origin.y}, {playhead, origin.y + 64}, IM_COL32(70, 220, 255, 255));
    ImGui::Text("0 — %.3f s", end);
    if (ImGui::BeginChild("action_rows", {0, 120}, ImGuiChildFlags_Borders)) {
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(track.Events().size()));
        while (clipper.Step())
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                const auto& action = track.Events()[row];
                const auto row_label = std::to_string(action.seconds_) + " s | " +
                                       kinds[static_cast<std::size_t>(action.kind_)] + " | " +
                                       std::to_string(action.value_) + "###action." +
                                       std::to_string(action.id_);
                if (ImGui::Selectable(row_label.c_str(), selected_ && selected_->id_ == action.id_))
                    selected_ = action;
            }
    }
    ImGui::EndChild();
    std::optional<parameters::EventTrack> result;
    auto actions =
            std::vector<parameters::RecordedEvent>(track.Events().begin(), track.Events().end());
    const auto publish = [&] {
        try {
            result.emplace(actions);
            error_.clear();
        } catch (const std::exception&) {
            error_ = "event.invalid_action";
        }
    };
    if (selected_) {
        ImGui::SetNextItemWidth(120);
        ImGui::InputDouble(label("key_time").c_str(), &selected_->seconds_, 0, 0, "%.6f");
        ImGui::SetNextItemWidth(160);
        if (ImGui::BeginCombo(label("event.action_kind").c_str(),
                              kinds[static_cast<std::size_t>(selected_->kind_)].c_str())) {
            for (std::size_t index = 0; index < kinds.size(); ++index)
                if (ImGui::Selectable(kinds[index].c_str(),
                                      index == static_cast<std::size_t>(selected_->kind_))) {
                    selected_->kind_ = static_cast<parameters::EventKind>(index);
                    if (selected_->kind_ != parameters::EventKind::kPulse) selected_->value_ = 1;
                }
            ImGui::EndCombo();
        }
        if (selected_->kind_ == parameters::EventKind::kGate) {
            bool on = selected_->value_ != 0;
            if (ImGui::Checkbox(label("event.gate_on").c_str(), &on))
                selected_->value_ = on ? 1 : 0;
        } else if (selected_->kind_ == parameters::EventKind::kPulse) {
            ImGui::SetNextItemWidth(120);
            ImGui::InputDouble(label("value").c_str(), &selected_->value_, 0, 0, "%.6f");
        }
        if (ImGui::Button(label("event.apply_action").c_str())) {
            for (auto& action : actions)
                if (action.id_ == selected_->id_) action = *selected_;
            publish();
        }
        ImGui::SameLine();
        if (ImGui::Button(label("event.remove_action").c_str())) {
            std::erase_if(actions,
                          [&](const auto& action) { return action.id_ == selected_->id_; });
            publish();
            selected_.reset();
        }
    }
    std::uint64_t last = 0;
    for (const auto& action : actions) last = std::max(last, action.id_);
    ImGui::BeginDisabled(actions.size() == parameters::EventTrack::kMaximumEvents ||
                         last == std::numeric_limits<std::uint64_t>::max());
    if (ImGui::Button(label("event.add_action").c_str())) {
        actions.push_back({last + 1, seconds, parameters::EventKind::kPulse, 1});
        publish();
        if (result) selected_ = actions.back();
    }
    ImGui::EndDisabled();
    if (!error_.empty()) ImGui::TextWrapped("%s", text.at(error_).c_str());
    if (result) previous_ = *result;
    ImGui::PopID();
    return result;
}
}  // namespace rhythm::studio
