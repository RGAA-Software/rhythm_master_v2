#include "asset_panel.h"

#include <imgui.h>

#include <algorithm>
#include <cctype>

#include "rhythm/project/package.h"

namespace rhythm::studio {
AssetEdit AssetPanel::Draw(const std::filesystem::path& directory,
                           std::span<const assets::AssetRecord> records,
                           const std::map<std::string, std::string>& catalog) {
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
    ImGui::SetNextWindowSize({560, 420}, ImGuiCond_FirstUseEver);
    if (!ImGui::BeginPopup("asset.manager.popup")) return edit;
    ImGui::TextWrapped("%s", text("asset.import_help").c_str());
    ImGui::BeginDisabled(importer_.Busy());
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("###asset.source", text("asset.source_path").c_str(), source_.data(),
                             source_.size());
    std::uint64_t total = 0;
    for (const auto& record : records) total += record.bytes_;
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
        importer_.Start(directory, source_path,
                        extension == ".glb" ? "model/gltf-binary" : "application/octet-stream",
                        project::kMaximumPackageAssetBytes - total);
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
