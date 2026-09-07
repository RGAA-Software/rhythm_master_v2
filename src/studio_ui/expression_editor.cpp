#include "expression_editor.h"

#include <imgui.h>

#include <algorithm>

namespace rhythm::studio {
std::optional<parameters::Expression> ExpressionEditor::Draw(
        const parameters::Expression& expression, const std::string& id,
        const std::map<std::string, std::string>& text) {
    if (id != id_ || expression.Source() != source_) {
        id_ = id;
        source_ = expression.Source();
        buffer_.fill(0);
        std::copy(source_.begin(), source_.end(), buffer_.begin());
        invalid_ = false;
    }
    ImGui::PushID(id.c_str());
    ImGui::TextWrapped("%s", text.at("expression.help").c_str());
    ImGui::SetNextItemWidth(-1);
    const bool enter = ImGui::InputText("###expression.source", buffer_.data(), buffer_.size(),
                                        ImGuiInputTextFlags_EnterReturnsTrue);
    const bool apply = ImGui::Button(text.at("expression.apply").c_str());
    std::optional<parameters::Expression> result;
    if (enter || apply) {
        try {
            parameters::Expression candidate(buffer_.data());
            invalid_ = false;
            if (candidate != expression) result = std::move(candidate);
        } catch (const std::exception&) {
            invalid_ = true;
        }
    }
    if (invalid_) ImGui::TextWrapped("%s", text.at("expression.invalid").c_str());
    ImGui::PopID();
    return result;
}
}  // namespace rhythm::studio
