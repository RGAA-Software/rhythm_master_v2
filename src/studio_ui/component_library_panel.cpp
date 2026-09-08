#include "component_library_panel.h"

#include <imgui.h>

#include <algorithm>
#include <iostream>

namespace rhythm::studio {
namespace {
std::string Utf8(const std::filesystem::path& path) {
    const auto bytes = path.u8string();
    return {bytes.begin(), bytes.end()};
}
}  // namespace
void ComponentLibraryPanel::Initialize(const std::filesystem::path& directory) {
    if (!library_) library_.emplace(directory);
}
std::optional<LibraryInsertion> ComponentLibraryPanel::Take() {
    if (!library_) return {};
    auto completed = library_->Take();
    if (!completed) return {};
    if (completed->entries_) entries_ = std::move(*completed->entries_);
    if (!completed->saved_.empty()) saved_ = completed->saved_;
    status_ = completed->error_;
    if (!status_.empty()) std::cerr << "component library: " << status_ << '\n';
    if (completed->component_ || !completed->error_.empty())
        return LibraryInsertion{std::move(*completed), insertion_};
    return {};
}
std::optional<LibraryRequest> ComponentLibraryPanel::Draw(
        const graph::Document& document, std::span<const graph::NodeId> selection,
        const std::map<std::string, std::string>& text) {
    if (!library_ ||
        !ImGui::CollapsingHeader(
                (text.at("component.user_library") + "###component.user_library").c_str()))
        return {};
    const auto label = [&](const std::string& key) { return text.at(key) + "###" + key; };
    ImGui::TextWrapped("%s", text.at("component.library_help").c_str());
    ImGui::TextWrapped("%s", Utf8(library_->Directory()).c_str());
    if (!status_.empty())
        ImGui::TextWrapped("%s",
                           text.at(text.contains(status_) ? status_ : "operation_failed").c_str());
    if (!saved_.empty())
        ImGui::TextWrapped("%s: %s", text.at("component.library_saved").c_str(),
                           Utf8(saved_).c_str());
    if (library_->Busy()) ImGui::TextUnformatted(text.at("component.library_busy").c_str());
    std::optional<LibraryRequest> request;
    ImGui::BeginDisabled(library_->Busy());
    bool selected_component = false;
    if (selection.size() == 1) {
        const auto node =
                std::find_if(document.nodes_.begin(), document.nodes_.end(),
                             [&](const auto& item) { return item.id_ == selection.front(); });
        selected_component =
                node != document.nodes_.end() &&
                std::any_of(document.components_.begin(), document.components_.end(),
                            [&](const auto& value) { return value.type_ == node->type_; });
    }
    ImGui::BeginDisabled(!selected_component);
    if (ImGui::Button(label("component.library_save").c_str())) {
        request = LibraryRequest{selection.front(), {}};
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button(label("component.library_refresh").c_str())) {
        library_->Refresh();
    }
    for (const auto& entry : entries_) {
        ImGui::PushID(entry.directory_.filename().string().c_str());
        if (ImGui::SmallButton(text.at("component.add").c_str()))
            request = LibraryRequest{{}, entry.directory_};
        ImGui::SameLine();
        ImGui::TextUnformatted(entry.title_.c_str());
        ImGui::PopID();
    }
    ImGui::InputTextWithHint("###component.library_path", text.at("component.library_path").c_str(),
                             import_path_.data(), import_path_.size());
    ImGui::BeginDisabled(!import_path_[0]);
    if (ImGui::Button(label("component.library_import").c_str())) {
        const std::string path(import_path_.data());
        request =
                LibraryRequest{{}, std::filesystem::path(std::u8string(path.begin(), path.end()))};
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    return request;
}
void ComponentLibraryPanel::Start(const LibraryRequest& request, const editor::Snapshot& snapshot,
                                  const std::filesystem::path& project_assets,
                                  editor::Position insertion) {
    if (!library_) return;
    bool accepted = false;
    if (request.save_instance_)
        accepted = library_->Save(snapshot, *request.save_instance_, project_assets);
    else {
        accepted = library_->Load(request.load_directory_, project_assets, snapshot.document_.id_,
                                  snapshot.document_.revision_);
        if (accepted) insertion_ = insertion;
    }
    status_ = accepted ? std::string{} : "component.library_busy";
}
bool ComponentLibraryPanel::StartOfficial(const content::Semantic& semantic,
                                          const editor::Snapshot& snapshot,
                                          const std::filesystem::path& project_assets,
                                          editor::Position insertion) {
    const bool accepted =
            library_ && library_->LoadOfficial(semantic, project_assets, snapshot.document_.id_,
                                               snapshot.document_.revision_);
    if (accepted) insertion_ = insertion;
    status_ = accepted ? std::string{} : "component.library_busy";
    return accepted;
}
}  // namespace rhythm::studio
