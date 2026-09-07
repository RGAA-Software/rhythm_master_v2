#include "curve_editor.h"

#include <imgui.h>

#include <array>
#include <stdexcept>

namespace rhythm::studio {
CurveEdit DrawCurveEditor(parameters::Curve& curve, const std::string& id,
                          const std::map<std::string, std::string>& text) {
    CurveEdit edit;
    ImGui::PushID(id.c_str());
    std::array<float, 128> plot{};
    const auto begin = curve.Keys().front().seconds_;
    const auto end = curve.Keys().back().seconds_;
    for (std::size_t index = 0; index < plot.size(); ++index)
        plot[index] = static_cast<float>(
                curve.Evaluate(begin + (end - begin) * index / (plot.size() - 1)));
    ImGui::PlotLines("##curve", plot.data(), static_cast<int>(plot.size()), 0, nullptr, FLT_MAX,
                     FLT_MAX, {0, 80});
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
                ImGui::SetNextItemWidth(100);
                edit.changed_ |= ImGui::InputDouble((text.at("key_time") + "##time").c_str(),
                                                    &key.seconds_, 0, 0, "%.3f");
                edit.committed_ |= ImGui::IsItemDeactivatedAfterEdit();
                ImGui::SetNextItemWidth(100);
                edit.changed_ |= ImGui::InputDouble((text.at("value") + "##value").c_str(),
                                                    &key.value_, 0, 0, "%.3f");
                edit.committed_ |= ImGui::IsItemDeactivatedAfterEdit();
                const std::array<std::string, 3> modes{
                        text.at("curve.step"), text.at("curve.linear"), text.at("curve.smooth")};
                ImGui::SetNextItemWidth(140);
                if (ImGui::BeginCombo("##interpolation",
                                      modes[static_cast<int>(key.interpolation_)].c_str())) {
                    for (int mode = 0; mode < 3; ++mode)
                        if (ImGui::Selectable(modes[mode].c_str(),
                                              mode == static_cast<int>(key.interpolation_))) {
                            key.interpolation_ = static_cast<parameters::Interpolation>(mode);
                            edit.changed_ = edit.committed_ = true;
                        }
                    ImGui::EndCombo();
                }
                ImGui::BeginDisabled(keys.size() == 1);
                const bool removed = ImGui::Button((text.at("remove_key") + "##remove").c_str());
                ImGui::EndDisabled();
                ImGui::Separator();
                ImGui::PopID();
                if (removed) {
                    keys.erase(keys.begin() + static_cast<std::ptrdiff_t>(index));
                    edit.changed_ = edit.committed_ = true;
                    removed_key = true;
                    break;
                }
            }
        }
    }
    ImGui::EndChild();
    ImGui::BeginDisabled(keys.size() == parameters::Curve::kMaximumKeys ||
                         keys.back().seconds_ > 1.0e9 - 1);
    if (ImGui::Button((text.at("add_key") + "##add").c_str())) {
        keys.push_back({keys.back().seconds_ + 1, keys.back().value_});
        edit.changed_ = edit.committed_ = true;
    }
    ImGui::EndDisabled();
    if (edit.changed_) {
        try {
            curve.SetKeys(std::move(keys));
        } catch (const std::exception&) {
            ImGui::TextWrapped("%s", text.at("invalid_curve").c_str());
            edit = {};
        }
    }
    ImGui::PopID();
    return edit;
}
}  // namespace rhythm::studio
