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
    // BeginPopup enables content autosizing; an explicit size each frame prevents
    // fill-available child regions and collapsed/filtered content from shrinking it.
    const auto available = ImGui::GetMainViewport()->WorkSize;
    ImGui::SetNextWindowSize({std::min(440.0f, std::max(1.0f, available.x - 16)),
                              std::min(480.0f, std::max(1.0f, available.y - 16))});
    if (!ImGui::BeginPopup("semantic.palette")) return {};
    ImGui::TextWrapped("%s", text.at("semantic.help").c_str());
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("###semantic.filter", text.at("semantic.search").c_str(),
                             filter_.data(), filter_.size());
    const auto query = Fold(filter_.data());
    std::optional<std::size_t> result;
    std::map<std::string, std::vector<std::size_t>> groups;
    for (std::size_t index = 0; index < entries.size(); ++index) {
        const auto& entry = entries[index];
        std::string searchable = entry.metadata_.id_;
        for (const auto& [language, title] : entry.metadata_.titles_) searchable += " " + title;
        const auto category = text.find("category." + entry.metadata_.category_);
        if (category != text.end()) searchable += " " + category->second;
        if (!query.empty() && Fold(searchable).find(query) == std::string::npos) continue;
        groups[entry.metadata_.category_].push_back(index);
    }
    for (const auto& [category, indices] : groups) {
        const auto title = text.find("category." + category);
        const auto label = (title == text.end() ? category : title->second) + "###" + category;
        if (!ImGui::CollapsingHeader(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) continue;
        for (const auto index : indices) {
            const auto& entry = entries[index];
            if (ImGui::Selectable(
                        (entry.metadata_.titles_.at(locale) + "###" + entry.metadata_.id_).c_str()))
                result = index;
        }
    }
    ImGui::EndPopup();
    return result;
}
}  // namespace rhythm::studio
