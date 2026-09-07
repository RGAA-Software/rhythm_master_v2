#include "asset_panel.h"

#include <imgui.h>

#include <algorithm>
#include <cctype>

#include "rhythm/project/package.h"

namespace rhythm::studio {
AssetEdit AssetPanel::Draw(const std::filesystem::path& directory,
                           std::span<const assets::AssetRecord> records,
                           const std::map<std::string, std::string>& catalog,
                           const std::optional<media::Soundtrack>& soundtrack) {
    const auto text = [&](const std::string& key) {
        const auto found = catalog.find(key);
        return found == catalog.end() ? catalog.at("operation_failed") : found->second;
    };
    AssetEdit edit;
    if (auto result = importer_.Take()) {
        if (result->asset_) {
            edit.added_ = std::move(result->asset_);
            status_ = "asset.imported";
        } else {
            status_ = result->error_;
        }
    }
    const auto label = text("asset.manager") + "###asset.manager";
    if (ImGui::Button(label.c_str())) ImGui::OpenPopup("asset.manager.popup");
    // BeginPopup enables content autosizing; an explicit size each frame prevents
    // fill-available child regions and collapsed/filtered content from shrinking it.
    const auto available = ImGui::GetMainViewport()->WorkSize;
    ImGui::SetNextWindowSize({std::min(560.0f, std::max(1.0f, available.x - 16)),
                              std::min(420.0f, std::max(1.0f, available.y - 16))});
    if (!ImGui::BeginPopup("asset.manager.popup")) return edit;
    ImGui::TextWrapped("%s", text("asset.import_help").c_str());
    ImGui::BeginDisabled(importer_.Busy());
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("###asset.source", text("asset.source_path").c_str(), source_.data(),
                             source_.size());
    std::uint64_t total = 0;
    std::uint64_t music_bytes = 0;
    for (const auto& record : records) {
        if (soundtrack && record.id_ == soundtrack->asset_)
            music_bytes = record.bytes_;
        else
            total += record.bytes_;
    }
    const bool full = total >= project::kMaximumPackageAssetBytes ||
                      records.size() >= project::kMaximumPackageAssets;
    ImGui::BeginDisabled(source_[0] == '\0' || full);
    if (ImGui::Button((text("asset.import") + "###asset.import").c_str())) {
        std::string source(source_.data());
        if (source.size() >= 2 && source.front() == '"' && source.back() == '"')
            source = source.substr(1, source.size() - 2);
        const std::filesystem::path source_path(std::u8string(source.begin(), source.end()));
        auto extension = source_path.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(),
                       [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
        const auto mime = extension == ".glb"                           ? "model/gltf-binary"
                          : extension == ".png"                         ? "image/png"
                          : extension == ".jpg" || extension == ".jpeg" ? "image/jpeg"
                          : extension == ".webp"                        ? "image/webp"
                          : extension == ".bmp"                         ? "image/bmp"
                          : extension == ".mp4"                         ? "video/mp4"
                          : extension == ".mkv"                         ? "video/x-matroska"
                          : extension == ".webm"                        ? "video/webm"
                          : extension == ".mov"                         ? "video/quicktime"
                                                : "application/octet-stream";
        importer_.Start(directory, source_path, mime, project::kMaximumPackageAssetBytes - total);
        status_ = "asset.importing";
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    if (importer_.Busy()) {
        ImGui::SameLine();
        if (ImGui::Button((text("asset.cancel") + "###asset.cancel").c_str())) importer_.Cancel();
    }
    if (full) ImGui::TextWrapped("%s", text("asset.package_full").c_str());
    if (!status_.empty()) ImGui::TextWrapped("%s", text(status_).c_str());
    ImGui::Separator();
    ImGui::Text("%s: %llu / 8 MiB", text("asset.total_bytes").c_str(),
                static_cast<unsigned long long>(total));
    if (soundtrack)
        ImGui::Text("%s: %llu / 256 MiB", text("asset.music_bytes").c_str(),
                    static_cast<unsigned long long>(music_bytes));
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(records.size()));
    while (clipper.Step()) {
        for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index) {
            const auto& record = records[static_cast<std::size_t>(index)];
            ImGui::PushID(record.id_.sha256_.c_str());
            ImGui::Text("%.12s  %llu B", record.id_.sha256_.c_str(),
                        static_cast<unsigned long long>(record.bytes_));
            ImGui::SameLine();
            if (ImGui::SmallButton(text("asset.remove").c_str())) edit.removed_ = record.id_;
            ImGui::PopID();
        }
    }
    ImGui::EndPopup();
    return edit;
}
}  // namespace rhythm::studio
