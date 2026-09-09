#include <imgui.h>

#include <algorithm>

#include "event_authoring.h"

namespace rhythm::studio {
std::optional<editor::Snapshot> EventAuthoring::Draw(
        const editor::Snapshot& snapshot, graph::NodeId selected,
        const std::map<std::string, std::string>& text) {
    const auto label = [&](const std::string& key) { return text.at(key) + "###" + key; };
    if (!Recording() && status_.empty() &&
        std::none_of(snapshot.document_.nodes_.begin(), snapshot.document_.nodes_.end(),
                     [&](const auto& node) {
                         return node.id_ == selected && node.type_ == "event.input";
                     }))
        return {};
    if (!ImGui::CollapsingHeader(label("event.live_actions").c_str(),
                                 ImGuiTreeNodeFlags_DefaultOpen))
        return {};
    const bool can_trigger =
            ready_ && available_.contains(selected) && (!Recording() || selected == target_) &&
            snapshot.document_.id_ == document_ && snapshot.document_.revision_ == revision_;
    if (!can_trigger) ImGui::TextWrapped("%s", text.at("event.select_live_input").c_str());
    ImGui::BeginDisabled(!can_trigger);
    if (ImGui::Button(label("event.pulse").c_str()))
        Trigger(selected, parameters::EventKind::kPulse, 1);
    ImGui::SameLine();
    if (ImGui::Button(label("event.gate_on").c_str()))
        Trigger(selected, parameters::EventKind::kGate, 1);
    ImGui::SameLine();
    if (ImGui::Button(label("event.gate_off").c_str()))
        Trigger(selected, parameters::EventKind::kGate, 0);
    if (ImGui::Button(label("event.reset_action").c_str()))
        Trigger(selected, parameters::EventKind::kReset, 1);
    ImGui::EndDisabled();
    std::optional<editor::Snapshot> result;
    if (Recording()) {
        ImGui::Text("%s: %zu", text.at("event.recorded_count").c_str(), Captured());
        ImGui::BeginDisabled(queue_.Pending() != 0);
        if (ImGui::Button(label("event.finish_recording").c_str())) result = Finish(snapshot);
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button(label("event.cancel_recording").c_str())) {
            Cancel();
            status_ = "event.take_cancelled";
        }
    } else {
        ImGui::BeginDisabled(!can_trigger || queue_.Pending() != 0);
        if (ImGui::Button(label("event.begin_recording").c_str())) Record(snapshot, selected);
        ImGui::EndDisabled();
    }
    if (!status_.empty()) ImGui::TextWrapped("%s", text.at(status_).c_str());
    ImGui::TextWrapped("%s", text.at("event.recording_help").c_str());
    return result;
}
}  // namespace rhythm::studio
