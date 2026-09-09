#include "rhythm/control_ui/control_panel.h"

#include <imgui.h>

#include <algorithm>

namespace rhythm::control_ui {
Edit ControlPanel::Draw(const parameters::ControlBank& bank,
                        const parameters::ControlValues& current,
                        const std::map<std::string, std::string>& text, bool authoring,
                        bool defer_recall) {
    Edit result;
    if (bank.Definitions().empty()) return result;
    const auto label = [&](const std::string& key) {
        const auto found = text.find(key);
        return (found == text.end() ? key : found->second) + "###" + key;
    };
    ImGui::PushID("public_controls");
    if (!ImGui::CollapsingHeader(label("controls.title").c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PopID();
        return result;
    }
    auto values = bank.Resolve(current);
    parameters::ControlValues changes;
    bool changed = false;
    ImGui::BeginChild("sliders",
                      {0, std::min(220.0f, static_cast<float>(bank.Definitions().size()) *
                                                   ImGui::GetFrameHeightWithSpacing())});
    for (const auto& control : bank.Definitions()) {
        auto value = values.at(control.id_);
        const auto name = control.title_ + "###macro." + std::to_string(control.id_);
        if (ImGui::SliderScalar(name.c_str(), ImGuiDataType_Double, &value, &control.minimum_,
                                &control.maximum_, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
            values[control.id_] = value;
            changes[control.id_] = value;
            changed = true;
        }
        result.committed_ |= ImGui::IsItemDeactivatedAfterEdit();
    }
    ImGui::EndChild();
    const auto snapshots = bank.Snapshots();
    if (!snapshots.empty()) {
        const auto choose = [&](const char* key, std::uint64_t& selected) {
            auto found = std::find_if(snapshots.begin(), snapshots.end(),
                                      [&](const auto& item) { return item.id_ == selected; });
            if (found == snapshots.end()) {
                selected = snapshots.front().id_;
                found = snapshots.begin();
            }
            if (ImGui::BeginCombo(label(key).c_str(), found->title_.c_str())) {
                for (const auto& item : snapshots) {
                    const auto name = item.title_ + "###snapshot." + std::to_string(item.id_);
                    if (ImGui::Selectable(name.c_str(), item.id_ == selected)) selected = item.id_;
                }
                ImGui::EndCombo();
            }
        };
        choose("controls.first", first_);
        choose("controls.second", second_);
        if (ImGui::Button(label("controls.recall").c_str())) {
            blend_ = 0;
            result.recall_ = first_;
            if (!defer_recall) {
                values = bank.Snapshot(first_);
                changes = values;
                changed = result.committed_ = true;
            }
        }
        if (authoring) {
            ImGui::SameLine();
            if (ImGui::Button(label("controls.remove").c_str())) result.remove_ = first_;
        }
        if (ImGui::SliderFloat(label("controls.blend").c_str(), &blend_, 0, 1, "%.3f",
                               ImGuiSliderFlags_AlwaysClamp)) {
            values = bank.Blend(bank.Snapshot(first_), bank.Snapshot(second_), blend_);
            changes = values;
            changed = true;
        }
        result.committed_ |= ImGui::IsItemDeactivatedAfterEdit();
    }
    if (authoring) {
        ImGui::InputText(label("controls.name").c_str(), name_.data(), name_.size());
        ImGui::BeginDisabled(name_[0] == '\0' ||
                             snapshots.size() >= parameters::ControlBank::kMaximumSnapshots);
        if (ImGui::Button(label("controls.capture").c_str())) {
            result.capture_ = name_.data();
            name_.fill(0);
        }
        ImGui::EndDisabled();
    }
    if (changed) result.values_ = std::move(changes);
    ImGui::PopID();
    return result;
}
}  // namespace rhythm::control_ui
