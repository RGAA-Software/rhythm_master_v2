#include "template_browser.h"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <fstream>

#include "rhythm/render/layout.h"

namespace rhythm::studio {
namespace {
std::string Fold(std::string value) {
    for (auto& character : value)
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    return value;
}
std::string Label(const std::map<std::string, std::string>& text, const std::string& key) {
    const auto found = text.find(key);
    return found == text.end() ? key : found->second;
}
}  // namespace
render::TextureHandle TemplateBrowser::Thumbnail(std::size_t index,
                                                 const project::ContentEntry& entry,
                                                 render::Renderer& renderer) {
    if (const auto found = thumbnails_.find(index); found != thumbnails_.end())
        return found->second.Handle();
    if (missing_thumbnails_.contains(index) || thumbnails_.size() >= 128) return {};
    std::ifstream file(entry.directory_ / "thumbnail.rgba", std::ios::binary | std::ios::ate);
    constexpr std::size_t kBytes = 256 * 144 * 4;
    if (!file || file.tellg() != static_cast<std::streamoff>(kBytes)) {
        missing_thumbnails_.insert(index);
        return {};
    }
    file.seekg(0);
    std::array<std::uint8_t, kBytes> pixels{};
    file.read(reinterpret_cast<char*>(pixels.data()), pixels.size());
    if (!file) {
        missing_thumbnails_.insert(index);
        return {};
    }
    auto texture = renderer.CreateTexture({256, 144}, pixels);
    const auto handle = texture.Handle();
    thumbnails_.emplace(index, std::move(texture));
    return handle;
}
void TemplateBrowser::UpdatePreview(std::span<const project::ContentEntry> entries,
                                    render::Renderer& renderer, double seconds,
                                    const runtime::ExternalInputs& inputs) {
    if (auto loaded = loader_.Take()) {
        if (requested_ == selected_) {
            if (loaded->package_) {
                // Take() returns ownership to the host thread; workers never touch GPU state.
                auto result = std::move(*loaded);
                preview_.LoadPrepared(std::move(*result.package_));
                playing_ = selected_;
            } else
                failed_ = true;
        }
        requested_.reset();
    }
    if (selected_ != playing_) {
        preview_ = {};
        live_ = {};
        playing_.reset();
        if (selected_ && !loader_.Busy() && !failed_) {
            const auto& entry = entries[*selected_];
            auto name = entry.directory_.filename();
            name += ".rhythmpack";
            if (loader_.StartFile(entry.directory_.parent_path().parent_path() / "packages" / name))
                requested_ = selected_;
        }
    }
    if (preview_.Ready()) {
        const auto fit = render::AspectFit(preview_.Canvas(), {0, 0, 256, 144});
        live_extent_ = {static_cast<std::uint16_t>(fit.width_),
                        static_cast<std::uint16_t>(fit.height_)};
        live_ = preview_.Tick(seconds, false, live_extent_, renderer, inputs).final_;
    }
}
std::optional<std::size_t> TemplateBrowser::Draw(std::span<const project::ContentEntry> entries,
                                                 const std::string& locale,
                                                 const std::map<std::string, std::string>& text,
                                                 platform::Host& host, render::Renderer& renderer,
                                                 double seconds,
                                                 const runtime::ExternalInputs& inputs) {
    if (ImGui::Button((text.at("templates") + "###templates").c_str()))
        ImGui::OpenPopup("templates.popup");
    // BeginPopup enables content autosizing; an explicit size each frame prevents
    // fill-available child regions and collapsed/filtered content from shrinking it.
    const auto available = ImGui::GetMainViewport()->WorkSize;
    ImGui::SetNextWindowSize({std::min(1040.0f, std::max(1.0f, available.x - 16)),
                              std::min(710.0f, std::max(1.0f, available.y - 16))});
    if (!ImGui::BeginPopup("templates.popup")) {
        loader_.Cancel();
        // Drain a cancelled request; it cannot replace a later selection.
        loader_.Take();
        requested_.reset();
        preview_ = {};
        playing_.reset();
        live_ = {};
        return {};
    }
    UpdatePreview(entries, renderer, seconds, inputs);
    ImGui::SetNextItemWidth(330);
    ImGui::InputTextWithHint("###template.search", text.at("catalog.search").c_str(),
                             filter_.data(), filter_.size());
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150);
    if (ImGui::BeginCombo("###template.tier",
                          Label(text, tier_.empty() ? "catalog.all" : "tier." + tier_).c_str())) {
        for (const std::string tier : {"", "basic", "advanced", "example"})
            if (ImGui::Selectable(
                        Label(text, tier.empty() ? "catalog.all" : "tier." + tier).c_str(),
                        tier_ == tier))
                tier_ = tier;
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(180);
    std::set<std::string> categories;
    for (const auto& entry : entries) categories.insert(entry.category_);
    if (ImGui::BeginCombo(
                "###template.category",
                Label(text, category_.empty() ? "catalog.subjects" : "category." + category_)
                        .c_str())) {
        if (ImGui::Selectable(text.at("catalog.subjects").c_str(), category_.empty()))
            category_.clear();
        for (const auto& category : categories)
            if (ImGui::Selectable(Label(text, "category." + category).c_str(),
                                  category_ == category))
                category_ = category;
        ImGui::EndCombo();
    }
    const auto query = Fold(filter_.data());
    ImGui::BeginChild("catalog.entries", {620, 0}, ImGuiChildFlags_Borders);
    std::size_t visible = 0;
    for (std::size_t index = 0; index < entries.size(); ++index) {
        const auto& entry = entries[index];
        std::string searchable = entry.id_ + " " + Label(text, "category." + entry.category_);
        for (const auto& [language, title] : entry.titles_) searchable += " " + title;
        for (const auto& [language, description] : entry.descriptions_)
            searchable += " " + description;
        if ((!tier_.empty() && entry.tier_ != tier_) ||
            (!category_.empty() && entry.category_ != category_) ||
            (!query.empty() && Fold(searchable).find(query) == std::string::npos))
            continue;
        ImGui::PushID(static_cast<int>(index));
        if (visible++ % 2) ImGui::SameLine();
        ImGui::BeginGroup();
        if (const auto texture = Thumbnail(index, entry, renderer); texture.device_) {
            if (ImGui::ImageButton("image", host.RegisterTexture(texture), {256, 144})) {
                selected_ = index;
                failed_ = false;
            }
        } else if (ImGui::Button(text.at("catalog.preview").c_str(), {264, 152})) {
            selected_ = index;
            failed_ = false;
        }
        if (ImGui::Selectable((entry.titles_.at(locale) + "###title").c_str(), selected_ == index,
                              ImGuiSelectableFlags_NoAutoClosePopups, {264, 0})) {
            selected_ = index;
            failed_ = false;
        }
        ImGui::TextDisabled("%s", Label(text, "tier." + entry.tier_).c_str());
        ImGui::EndGroup();
        ImGui::PopID();
    }
    if (!visible) ImGui::TextUnformatted(text.at("catalog.empty").c_str());
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("catalog.detail", {0, 0});
    std::optional<std::size_t> result;
    if (selected_) {
        const auto& entry = entries[*selected_];
        ImGui::TextWrapped("%s", entry.titles_.at(locale).c_str());
        if (live_.device_ && playing_ == selected_) {
            const auto fit = render::AspectFit(live_extent_, {0, 0, 320, 180});
            ImGui::Image(host.RegisterTexture(live_), {fit.width_, fit.height_});
        } else
            ImGui::TextWrapped("%s",
                               text.at(failed_ ? "catalog.failed" : "catalog.loading").c_str());
        if (const auto found = entry.descriptions_.find(locale); found != entry.descriptions_.end())
            ImGui::TextWrapped("%s", found->second.c_str());
        ImGui::TextWrapped("%s", text.at("catalog.live_help").c_str());
        if (ImGui::Button(text.at("catalog.use").c_str())) {
            result = selected_;
            ImGui::CloseCurrentPopup();
        }
    } else
        ImGui::TextWrapped("%s", text.at("catalog.select").c_str());
    ImGui::EndChild();
    ImGui::EndPopup();
    return result;
}
}  // namespace rhythm::studio
