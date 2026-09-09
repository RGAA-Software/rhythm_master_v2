#include "asset_panel.h"

#include <imgui.h>

#include <algorithm>
#include <cctype>

#include "rhythm/project/package.h"

namespace rhythm::studio {
namespace {
std::filesystem::path SourcePath(const std::string& input) {
    auto source = input;
    if (source.size() >= 2 && source.front() == '"' && source.back() == '"')
        source = source.substr(1, source.size() - 2);
    return std::filesystem::path(std::u8string(source.begin(), source.end()));
}
std::string MediaType(const std::filesystem::path& source) {
    auto extension = source.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    const std::map<std::string, std::string> types{
            {".glb", "model/gltf-binary"}, {".png", "image/png"},        {".jpg", "image/jpeg"},
            {".jpeg", "image/jpeg"},       {".webp", "image/webp"},      {".bmp", "image/bmp"},
            {".mp4", "video/mp4"},         {".mkv", "video/x-matroska"}, {".webm", "video/webm"},
            {".mov", "video/quicktime"},   {".otf", "font/otf"},         {".ttf", "font/ttf"},
            {".txt", "text/plain"}};
    const auto found = types.find(extension);
    return found == types.end() ? "application/octet-stream" : found->second;
}
}  // namespace
AssetEdit AssetPanel::Draw(const std::filesystem::path& directory, const editor::Snapshot& snapshot,
                           const std::map<std::string, std::string>& catalog) {
    const auto text = [&](const std::string& key) {
        const auto found = catalog.find(key);
        return found == catalog.end() ? catalog.at("operation_failed") : found->second;
    };
    AssetEdit edit;
    const auto& records = snapshot.assets_;
    if (directory_ != directory || project_id_ != snapshot.document_.id_) {
        discard_pending_ = importer_.Busy();
        importer_.Cancel();
        replacing_.reset();
        checks_.clear();
        selected_ = {};
        status_.clear();
        directory_ = directory;
        project_id_ = snapshot.document_.id_;
    } else if (auto result = importer_.Take()) {
        if (discard_pending_) {
            discard_pending_ = false;
        } else if (!result->imported_.empty()) {
            edit.added_ = result->imported_.front();
            edit.companions_.assign(result->imported_.begin() + 1, result->imported_.end());
            selected_ = edit.added_->id_;
            checks_.clear();
            status_ = "asset.imported";
        } else if (result->asset_) {
            edit.added_ = result->asset_;
            if (replacing_) edit.replaced_ = replacing_->id_;
            selected_ = result->asset_->id_;
            checks_.clear();
            status_ = replacing_ ? "asset.replaced" : "asset.imported";
        } else if (result->restored_) {
            edit.restored_ = result->restored_->id_;
            checks_.clear();
            status_ = "asset.restored";
        } else if (result->error_.empty()) {
            checks_ = std::move(result->checks_);
            edit.checked_ = checks_;
            status_ = "asset.checked";
        } else
            status_ = result->error_;
        replacing_.reset();
    }
    const auto label = text("asset.manager") + "###asset.manager";
    if (ImGui::Button(label.c_str())) ImGui::OpenPopup("asset.manager.popup");
    // Explicit size prevents fill-available regions from shrinking this popup.
    const auto available = ImGui::GetMainViewport()->WorkSize;
    ImGui::SetNextWindowSize({std::min(720.0f, std::max(1.0f, available.x - 16)),
                              std::min(600.0f, std::max(1.0f, available.y - 16))});
    if (!ImGui::BeginPopup("asset.manager.popup")) return edit;
    ImGui::TextWrapped("%s", text("asset.import_help").c_str());
    ImGui::BeginDisabled(importer_.Busy());
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("###asset.source", text("asset.source_path").c_str(), source_.data(),
                             source_.size());
    std::uint64_t total = 0;
    std::uint64_t music_bytes = 0;
    const auto& soundtrack = snapshot.soundtrack_;
    for (const auto& record : records) {
        if (soundtrack && soundtrack->clips_.empty() && record.id_ == soundtrack->asset_)
            music_bytes = record.bytes_;
        else
            total += record.bytes_;
    }
    const bool full = total >= project::kMaximumPackageAssetBytes ||
                      records.size() >= project::kMaximumPackageAssets;
    ImGui::BeginDisabled(source_[0] == '\0' || full);
    if (ImGui::Button((text("asset.import") + "###asset.import").c_str())) {
        const auto source = SourcePath(source_.data());
        replacing_.reset();
        if (importer_.Start(directory, source, MediaType(source),
                            project::kMaximumPackageAssetBytes - total))
            status_ = "asset.importing";
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(builtin_font_.empty() ||
                         records.size() + 2 > project::kMaximumPackageAssets || full);
    if (ImGui::Button((text("text.add_builtin_font") + "###text.add_builtin_font").c_str())) {
        replacing_.reset();
        if (importer_.StartBundle(directory,
                                  {{builtin_font_, "font/otf"}, {builtin_notice_, "text/plain"}},
                                  project::kMaximumPackageAssetBytes - total))
            status_ = "asset.importing";
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button((text("asset.check") + "###asset.check").c_str())) {
        replacing_.reset();
        status_ =
                importer_.StartInspect(directory, records) ? "asset.checking" : "asset.check_limit";
    }
    ImGui::EndDisabled();
    if (importer_.Busy()) {
        ImGui::SameLine();
        if (ImGui::Button((text("asset.cancel") + "###asset.cancel").c_str())) importer_.Cancel();
    }
    if (full) ImGui::TextWrapped("%s", text("asset.package_full").c_str());
    if (!status_.empty()) ImGui::TextWrapped("%s", text(status_).c_str());
    ImGui::Text(
            "%s: %llu / %llu MiB", text("asset.total_bytes").c_str(),
            static_cast<unsigned long long>(total),
            static_cast<unsigned long long>(project::kMaximumPackageAssetBytes / (1024 * 1024)));
    if (music_bytes)
        ImGui::Text("%s: %llu / 256 MiB", text("asset.music_bytes").c_str(),
                    static_cast<unsigned long long>(music_bytes));
    if (ImGui::BeginChild("###asset.records", {0, 170}, ImGuiChildFlags_Borders)) {
        for (const auto& record : records) {
            const auto check = std::find_if(checks_.begin(), checks_.end(), [&](const auto& value) {
                return value.asset_ == record;
            });
            const auto health = check == checks_.end()                          ? "asset.unchecked"
                                : check->health_ == assets::AssetHealth::kValid ? "asset.valid"
                                : check->health_ == assets::AssetHealth::kMissing ? "asset.missing"
                                                                                  : "asset.corrupt";
            const auto row = record.media_type_ + "  " + record.id_.sha256_.substr(0, 12) + "  " +
                             std::to_string(record.bytes_) + " B  " + text(health) + "###" +
                             record.id_.sha256_;
            if (ImGui::Selectable(row.c_str(), selected_ == record.id_)) selected_ = record.id_;
        }
    }
    ImGui::EndChild();
    const auto selected = std::find_if(records.begin(), records.end(),
                                       [&](const auto& record) { return record.id_ == selected_; });
    if (selected != records.end()) {
        const auto uses = editor::AssetUses(snapshot, selected_);
        ImGui::Text("%s: %zu", text("asset.references").c_str(), uses.size());
        ImGui::BeginDisabled(importer_.Busy());
        const bool replaceable = selected->media_type_.starts_with("image/") ||
                                 selected->media_type_.starts_with("font/") ||
                                 selected->media_type_.starts_with("video/") ||
                                 selected->media_type_ == "model/gltf-binary";
        const auto without = total >= selected->bytes_ ? total - selected->bytes_ : total;
        ImGui::BeginDisabled(source_[0] == '\0' || !replaceable ||
                             without >= project::kMaximumPackageAssetBytes);
        if (ImGui::Button((text("asset.replace") + "###asset.replace").c_str())) {
            const auto source = SourcePath(source_.data());
            if (importer_.Start(directory, source, MediaType(source),
                                project::kMaximumPackageAssetBytes - without)) {
                replacing_ = *selected;
                status_ = "asset.importing";
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(source_[0] == '\0');
        if (ImGui::Button((text("asset.restore") + "###asset.restore").c_str())) {
            replacing_.reset();
            if (importer_.StartRestore(directory, SourcePath(source_.data()), *selected))
                status_ = "asset.restoring";
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!uses.empty());
        if (ImGui::Button((text("asset.remove") + "###asset.remove").c_str()) && uses.empty() &&
            !importer_.Busy())
            edit.removed_ = selected_;
        ImGui::EndDisabled();
        ImGui::EndDisabled();
        if (!replaceable) ImGui::TextWrapped("%s", text("asset.specialized_edit").c_str());
        if (ImGui::BeginChild("###asset.usages", {0, 90}, ImGuiChildFlags_Borders)) {
            for (const auto& use : uses) {
                if (use.kind_ == editor::AssetUseKind::kNode)
                    ImGui::Text("%s / %llu / %s",
                                use.component_.empty() ? text("asset.root_graph").c_str()
                                                       : use.component_.c_str(),
                                static_cast<unsigned long long>(use.node_), use.property_.c_str());
                else if (use.kind_ == editor::AssetUseKind::kSoundtrack)
                    ImGui::TextUnformatted(text("asset.music_bytes").c_str());
                else
                    ImGui::Text("%s %llu", text("asset.audio_clip").c_str(),
                                static_cast<unsigned long long>(use.clip_));
            }
        }
        ImGui::EndChild();
    }
    ImGui::EndPopup();
    return edit;
}
}  // namespace rhythm::studio
