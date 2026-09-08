#include "curve_editor.h"

#include <imgui.h>

#include <array>
#include <stdexcept>

namespace rhythm::studio {
void CurveEditor::Reset() {
    previous_.reset();
    selected_.clear();
    dragged_.reset();
    drag_changed_ = false;
    transform_ = {};
    error_.clear();
}
CurveEdit CurveEditor::Draw(parameters::Curve& curve, const std::string& id,
                            const std::map<std::string, std::string>& text) {
    if (id != id_ || (previous_ && *previous_ != curve)) Reset();
    id_ = id;
    ImGui::PushID(id.c_str());
    auto edit = DrawPlot(curve);
    auto keys = std::vector<parameters::Keyframe>(curve.Keys().begin(), curve.Keys().end());
    if (ImGui::BeginChild("keys", {0, 180}, ImGuiChildFlags_Borders)) {
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(keys.size()));
        bool removed_key = false;
        while (clipper.Step() && !removed_key) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                const auto index = static_cast<std::size_t>(row);
                ImGui::PushID(static_cast<int>(index));
                auto& key = keys[index];
                bool selected = selected_.contains(index);
                if (ImGui::Checkbox("##selected", &selected)) {
                    if (selected)
                        selected_.insert(index);
                    else
                        selected_.erase(index);
                }
                ImGui::SameLine();
                ImGui::SetNextItemWidth(100);
                edit.changed_ |= ImGui::InputDouble((text.at("key_time") + "##time").c_str(),
                                                    &key.seconds_, 0, 0, "%.3f");
                edit.committed_ |= ImGui::IsItemDeactivatedAfterEdit();
                ImGui::SetNextItemWidth(100);
                edit.changed_ |= ImGui::InputDouble((text.at("value") + "##value").c_str(),
                                                    &key.value_, 0, 0, "%.3f");
                edit.committed_ |= ImGui::IsItemDeactivatedAfterEdit();
                const std::array<std::string, 4> modes{
                        text.at("curve.step"), text.at("curve.linear"), text.at("curve.smooth"),
                        text.at("curve.hermite")};
                ImGui::SetNextItemWidth(140);
                if (ImGui::BeginCombo("##interpolation",
                                      modes[static_cast<int>(key.interpolation_)].c_str())) {
                    for (int mode = 0; mode < 4; ++mode)
                        if (ImGui::Selectable(modes[mode].c_str(),
                                              mode == static_cast<int>(key.interpolation_))) {
                            key.interpolation_ = static_cast<parameters::Interpolation>(mode);
                            edit.changed_ = edit.committed_ = true;
                        }
                    ImGui::EndCombo();
                }
                // All rows have equal height so ImGuiListClipper remains correct.
                ImGui::BeginDisabled(key.interpolation_ != parameters::Interpolation::kHermite &&
                                     (!index || keys[index - 1].interpolation_ !=
                                                        parameters::Interpolation::kHermite));
                ImGui::SetNextItemWidth(100);
                edit.changed_ |= ImGui::InputDouble(text.at("curve.in_slope").c_str(),
                                                    &key.in_slope_, 0, 0, "%.3f");
                edit.committed_ |= ImGui::IsItemDeactivatedAfterEdit();
                ImGui::SetNextItemWidth(100);
                edit.changed_ |= ImGui::InputDouble(text.at("curve.out_slope").c_str(),
                                                    &key.out_slope_, 0, 0, "%.3f");
                edit.committed_ |= ImGui::IsItemDeactivatedAfterEdit();
                ImGui::EndDisabled();
                ImGui::BeginDisabled(keys.size() == 1);
                const bool removed = ImGui::Button((text.at("remove_key") + "##remove").c_str());
                ImGui::EndDisabled();
                ImGui::Separator();
                ImGui::PopID();
                if (removed) {
                    keys.erase(keys.begin() + static_cast<std::ptrdiff_t>(index));
                    selected_.clear();
                    edit.changed_ = edit.committed_ = true;
                    removed_key = true;
                    break;
                }
            }
        }
    }
    ImGui::EndChild();
    if (edit.changed_) {
        try {
            curve.SetKeys(keys);
            error_.clear();
        } catch (const std::exception&) {
            error_ = "invalid_curve";
            edit = {};
        }
    }
    if (DrawBatch(curve, text)) edit.changed_ = edit.committed_ = true;
    keys.assign(curve.Keys().begin(), curve.Keys().end());
    if (!error_.empty()) ImGui::TextWrapped("%s", text.at(error_).c_str());
    ImGui::BeginDisabled(keys.size() == parameters::Curve::kMaximumKeys ||
                         keys.back().seconds_ > 1.0e9 - 1);
    if (ImGui::Button((text.at("add_key") + "##add").c_str())) {
        keys.push_back({keys.back().seconds_ + 1, keys.back().value_});
        curve.SetKeys(keys);
        edit.changed_ = edit.committed_ = true;
    }
    ImGui::EndDisabled();
    previous_ = curve;
    ImGui::PopID();
    return edit;
}
}  // namespace rhythm::studio
