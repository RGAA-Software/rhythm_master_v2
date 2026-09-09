#include "text_editor.h"

#include <imgui.h>

#include <algorithm>

#include "rhythm/graph/text.h"

namespace rhythm::studio {
std::optional<std::string> TextEditor::Draw(const std::string& source, const std::string& id,
                                            const std::map<std::string, std::string>& text) {
    const auto reset = [&] {
        buffer_.fill(0);
        std::copy_n(source.begin(), std::min(source.size(), buffer_.size() - 1), buffer_.begin());
        invalid_ = false;
    };
    if (id != id_ || source != source_) {
        id_ = id;
        source_ = source;
        reset();
    }
    ImGui::TextWrapped("%s", text.at("text.edit_help").c_str());
    const bool enter = ImGui::InputTextMultiline(
            id.c_str(), buffer_.data(), buffer_.size(), {0, ImGui::GetTextLineHeight() * 5},
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CtrlEnterForNewLine);
    ImGui::PushID(id.c_str());
    const bool apply = ImGui::Button((text.at("text.apply") + "###text.apply").c_str());
    ImGui::SameLine();
    if (ImGui::Button((text.at("text.cancel") + "###text.cancel").c_str())) reset();
    ImGui::PopID();
    std::optional<std::string> result;
    if (enter || apply) {
        std::string candidate(buffer_.data());
        invalid_ = !graph::ValidText(candidate);
        if (!invalid_ && candidate != source) result = std::move(candidate);
    }
    if (invalid_) ImGui::TextWrapped("%s", text.at("text.invalid_content").c_str());
    return result;
}
}  // namespace rhythm::studio
