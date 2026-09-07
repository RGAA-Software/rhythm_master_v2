#include "component_workbench.h"

#include <imgui.h>

#include <algorithm>

namespace rhythm::studio {
void ComponentWorkbench::Open(const editor::Snapshot& project, std::string type) {
    edit_.emplace(project, std::move(type));
    status_.clear();
    ResetView();
}
void ComponentWorkbench::CommitPreview() {
    if (edit_ && inspector_.Preview()) edit_->ReplaceBody(*inspector_.Preview());
    inspector_.Reset();
}
void ComponentWorkbench::ResetView() {
    inspector_.Reset();
    canvas_.RestoreLayout();
}
std::optional<editor::Snapshot> ComponentWorkbench::Draw(
        const editor::Snapshot& project, const graph::Registry& registry,
        const std::map<std::string, std::string>& text, const std::string& locale) {
    if (!edit_) return {};
    const auto label = [&](const std::string& key) {
        const auto found = text.find(key);
        return found == text.end() ? key : found->second;
    };
    bool open = true;
    std::optional<editor::Snapshot> result;
    ImGui::SetNextWindowSize({1100, 750}, ImGuiCond_FirstUseEver);
    if (ImGui::Begin((label("component.edit") + "###component.workbench").c_str(), &open)) {
        ImGui::TextWrapped("%s", label("component.draft_help").c_str());
        if (ImGui::Button(label("component.apply").c_str())) {
            CommitPreview();
            auto applied = edit_->Finish(project, registry);
            if (std::holds_alternative<editor::Snapshot>(applied)) {
                result = std::get<editor::Snapshot>(std::move(applied));
                open = false;
            } else
                status_ = label(std::get<graph::Diagnostic>(applied).code_);
        }
        ImGui::SameLine();
        if (ImGui::Button(label("component.cancel").c_str())) open = false;
        ImGui::SameLine();
        if (ImGui::Button(label("undo").c_str())) {
            CommitPreview();
            edit_->Undo();
            ResetView();
        }
        ImGui::SameLine();
        if (ImGui::Button(label("redo").c_str())) {
            CommitPreview();
            edit_->Redo();
            ResetView();
        }
        const auto path = edit_->Path();
        for (std::size_t index = 0; index < path.size(); ++index) {
            ImGui::SameLine();
            if (ImGui::SmallButton(
                        (label(path[index]) + "###crumb" + std::to_string(index)).c_str())) {
                CommitPreview();
                edit_->Navigate(index);
                ResetView();
                break;
            }
        }
        if (!status_.empty()) ImGui::TextWrapped("%s", status_.c_str());
        if (ImGui::Button(label("component.enter").c_str())) {
            CommitPreview();
            if (edit_->Enter(canvas_.Selection())) ResetView();
        }
        ImGui::SameLine();
        if (ImGui::Button(label("component.set_output").c_str())) {
            CommitPreview();
            auto body = edit_->Body();
            if (std::any_of(body.document_.nodes_.begin(), body.document_.nodes_.end(),
                            [&](const auto& node) { return node.id_ == canvas_.Selection(); })) {
                body.document_.output_ = canvas_.Selection();
                edit_->ReplaceBody(std::move(body));
            }
        }
        ImGui::SameLine();
        std::vector<graph::OperatorDescriptor> available_nodes(registry.Operators().begin(),
                                                               registry.Operators().end());
        const auto body = edit_->Body();
        for (const auto& definition : body.document_.components_) {
            if (std::find(edit_->Path().begin(), edit_->Path().end(), definition.type_) !=
                edit_->Path().end())
                continue;
            if (auto descriptor = registry.Find(definition.type_, body.document_.components_))
                available_nodes.push_back(std::move(*descriptor));
        }
        if (const auto type = node_palette_.Draw(available_nodes, text, "component.add")) {
            CommitPreview();
            const auto id = edit_->ReserveNodeId();
            auto added =
                    editor::AddNode(edit_->Body(), registry, *type, canvas_.InsertionPoint(), id);
            if (std::holds_alternative<editor::Snapshot>(added)) {
                edit_->ReplaceBody(std::get<editor::Snapshot>(std::move(added)));
                ResetView();
                canvas_.Select(id);
            } else {
                status_ = label(std::get<graph::Diagnostic>(added).code_);
            }
        }
        ImGui::SameLine();
        ImGui::Text("%s: %llu", label("output").c_str(),
                    static_cast<unsigned long long>(edit_->Definition().output_));
        const auto available = ImGui::GetContentRegionAvail();
        if (ImGui::BeginChild("component.canvas", {std::max(250.0f, available.x * 0.62f), 0})) {
            ImGui::BeginDisabled(inspector_.Preview().has_value());
            if (const auto edited = canvas_.Draw(edit_->Body(), registry, text)) {
                edit_->ReplaceBody(*edited);
            }
            ImGui::EndDisabled();
        }
        ImGui::EndChild();
        ImGui::SameLine();
        if (ImGui::BeginChild("component.inspector")) {
            const auto inspected =
                    inspector_.Draw(edit_->Body(), canvas_.Selection(), registry, {}, text, locale);
            if (inspected.committed_) edit_->ReplaceBody(*inspected.committed_);
            if (const auto definition =
                        interface_.Draw(edit_->Definition(), edit_->Body().document_,
                                        canvas_.Selection(), registry, text)) {
                CommitPreview();
                edit_->ReplaceInterface(*definition);
            }
        }
        ImGui::EndChild();
    }
    ImGui::End();
    if (!open) {
        edit_.reset();
        inspector_.Reset();
    }
    return result;
}
}  // namespace rhythm::studio
