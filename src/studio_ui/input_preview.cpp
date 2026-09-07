#include "input_preview.h"

#include <imgui.h>

#include <algorithm>

namespace rhythm::studio {
void InputPreview::Draw(const std::map<std::string, std::string>& text) {
    if (!ImGui::CollapsingHeader((text.at("inputs.preview") + "###inputs.preview").c_str())) return;
    ImGui::TextWrapped("%s", text.at("inputs.help").c_str());
    ImGui::Checkbox((text.at("inputs.enabled") + "###inputs.enabled").c_str(), &enabled_);
    ImGui::BeginDisabled(!enabled_);
    auto index = static_cast<int>(values_.index_);
    auto group = static_cast<int>(values_.group_);
    if (ImGui::InputInt((text.at("role.index") + "###inputs.index").c_str(), &index))
        values_.index_ = static_cast<std::uint32_t>(std::clamp(index, 0, 65535));
    if (ImGui::InputInt((text.at("role.group") + "###inputs.group").c_str(), &group))
        values_.group_ = static_cast<std::uint32_t>(std::clamp(group, 0, 65535));
    ImGui::SliderInt((text.at("channel") + "###inputs.channel").c_str(), &channel_, 0, 31, "%d",
                     ImGuiSliderFlags_AlwaysClamp);
    constexpr double kMinimum = 0;
    constexpr double kMaximum = 1;
    ImGui::SliderScalar((text.at("inputs.control") + "###inputs.control").c_str(),
                        ImGuiDataType_Double,
                        &values_.controls_.at(static_cast<std::size_t>(channel_)), &kMinimum,
                        &kMaximum, "%.3f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::Checkbox((text.at("inputs.session_clock") + "###inputs.session_clock").c_str(),
                    &session_clock_);
    if (session_clock_) {
        constexpr double kMinimumOffset = -86400;
        constexpr double kMaximumOffset = 86400;
        ImGui::DragScalar((text.at("inputs.offset") + "###inputs.offset").c_str(),
                          ImGuiDataType_Double, &offset_seconds_, 0.1f, &kMinimumOffset,
                          &kMaximumOffset, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    }
    ImGui::EndDisabled();
}
runtime::ExternalInputs InputPreview::Snapshot(double local_seconds) const {
    runtime::ExternalInputs result;
    if (enabled_) {
        result.participant_ = values_;
        if (session_clock_)
            result.session_seconds_ = std::max(0.0, local_seconds + offset_seconds_);
    }
    return result;
}
}  // namespace rhythm::studio
