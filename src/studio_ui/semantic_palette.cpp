#include "semantic_palette.h"

#include <imgui.h>

#include <algorithm>
#include <cctype>

#include "operator_help.h"
#include "rhythm/render/layout.h"

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
                                                 const std::map<std::string, std::string>& text,
                                                 platform::Host& host, render::Renderer& renderer,
                                                 double seconds,
                                                 const runtime::ExternalInputs& inputs) {
    if (ImGui::Button((text.at("semantic.library") + "###semantic.library").c_str()))
        ImGui::OpenPopup("semantic.palette");
    // BeginPopup enables content autosizing; an explicit size each frame prevents
    // fill-available child regions and collapsed/filtered content from shrinking it.
    const auto available = ImGui::GetMainViewport()->WorkSize;
    ImGui::SetNextWindowSize({std::min(960.0f, std::max(1.0f, available.x - 16)),
                              std::min(680.0f, std::max(1.0f, available.y - 16))});
    if (!ImGui::BeginPopup("semantic.palette")) {
        preview_.Close();
        return {};
    }
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
        for (const auto& [language, description] : entry.metadata_.descriptions_)
            searchable += " " + description;
        const auto category = text.find("category." + entry.metadata_.category_);
        if (category != text.end()) searchable += " " + category->second;
        if (!query.empty() && Fold(searchable).find(query) == std::string::npos) continue;
        groups[entry.metadata_.category_].push_back(index);
    }
    ImGui::BeginChild("semantic.entries",
                      {std::min(330.0f, ImGui::GetContentRegionAvail().x * 0.4f), 0},
                      ImGuiChildFlags_Borders);
    if (groups.empty()) ImGui::TextWrapped("%s", text.at("catalog.empty").c_str());
    for (const auto& [category, indices] : groups) {
        const auto title = text.find("category." + category);
        const auto label = (title == text.end() ? category : title->second) + "###" + category;
        if (!query.empty()) ImGui::SetNextItemOpen(true, ImGuiCond_Always);
        if (!ImGui::CollapsingHeader(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) continue;
        for (const auto index : indices) {
            const auto& entry = entries[index];
            if (ImGui::Selectable(
                        (entry.metadata_.titles_.at(locale) + "###" + entry.metadata_.id_).c_str(),
                        selected_ == index, ImGuiSelectableFlags_NoAutoClosePopups))
                selected_ = index;
        }
    }
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("semantic.detail", {0, 0});
    if (selected_ && *selected_ < entries.size()) {
        const auto& entry = entries[*selected_];
        auto name = entry.metadata_.directory_.filename();
        name += ".rhythmpack";
        preview_.Select(entry.metadata_.directory_.parent_path().parent_path() /
                        "semantic_packages" / name);
        preview_.Update(renderer, seconds, inputs);
        ImGui::TextWrapped("%s", entry.metadata_.titles_.at(locale).c_str());
        if (const auto texture = preview_.Texture(); texture.device_) {
            const auto width = std::max(1.0f, ImGui::GetContentRegionAvail().x);
            const auto fit = render::AspectFit(preview_.Extent(), {0, 0, width, width * 0.5625f});
            ImGui::Image(host.RegisterTexture(texture), {fit.width_, fit.height_});
        } else
            ImGui::TextWrapped(
                    "%s",
                    text.at(preview_.Failed() ? "catalog.failed" : "catalog.loading").c_str());
        if (preview_.Failed() && ImGui::Button(text.at("render.retry").c_str())) preview_.Retry();
        if (const auto found = entry.metadata_.descriptions_.find(locale);
            found != entry.metadata_.descriptions_.end())
            ImGui::TextWrapped("%s", found->second.c_str());
        ImGui::TextWrapped("%s", text.at("semantic.preview_help").c_str());
        if (ImGui::Button((text.at("semantic.insert") + "###semantic.insert").c_str())) {
            result = selected_;
            ImGui::CloseCurrentPopup();
        }
        if (const auto descriptor =
                    registry_.Find(entry.root_.type_, entry.content_.document_.components_))
            DrawOperatorHelp(*descriptor, text);
    } else
        ImGui::TextWrapped("%s", text.at("semantic.select").c_str());
    ImGui::EndChild();
    ImGui::EndPopup();
    return result;
}
}  // namespace rhythm::studio
