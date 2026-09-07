#include "semantic_palette.h"

#include <imgui.h>

#include <algorithm>
#include <cctype>

namespace rhythm::studio {
namespace {
std::string Fold(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}
}  // namespace
std::optional<std::size_t> SemanticPalette::Draw(std::span<const content::Semantic> entries,
                                                 const std::string& locale,
                                                 const std::map<std::string, std::string>& text) {
    if (ImGui::Button((text.at("semantic.library") + "###semantic.library").c_str()))
        ImGui::OpenPopup("semantic.palette");
    ImGui::SetNextWindowSize({440, 480}, ImGuiCond_FirstUseEver);
    if (!ImGui::BeginPopup("semantic.palette")) return {};
    ImGui::TextWrapped("%s", text.at("semantic.help").c_str());
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("###semantic.filter", text.at("semantic.search").c_str(),
                             filter_.data(), filter_.size());
    const auto query = Fold(filter_.data());
    std::optional<std::size_t> result;
    for (std::size_t index = 0; index < entries.size(); ++index) {
        const auto& entry = entries[index];
        std::string searchable = entry.metadata_.id_;
        for (const auto& [language, title] : entry.metadata_.titles_) searchable += " " + title;
        if (!query.empty() && Fold(searchable).find(query) == std::string::npos) continue;
        if (ImGui::Selectable(
                    (entry.metadata_.titles_.at(locale) + "###" + entry.metadata_.id_).c_str()))
            result = index;
    }
    ImGui::EndPopup();
    return result;
}
}  // namespace rhythm::studio
