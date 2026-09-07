#include "component_panel.h"

#include <imgui.h>

#include <algorithm>

namespace rhythm::studio {
std::optional<ComponentAction> ComponentPanel::Draw(
        const graph::Document& document, std::span<const graph::NodeId> selection,
        const std::map<std::string, std::string>& text) {
    const auto label = [&](const std::string& key) {
        const auto found = text.find(key);
        return found == text.end() ? key : found->second;
    };
    if (!ImGui::CollapsingHeader((label("component.title") + "###component.panel").c_str()))
        return {};
    ImGui::TextWrapped("%s", label("component.help").c_str());
    ImGui::InputText((label("component.name") + "###component.name").c_str(), name_.data(),
                     name_.size());
    ImGui::BeginDisabled(selection.empty() || !name_[0]);
    const bool create = ImGui::Button((label("component.create") + "###component.create").c_str());
    ImGui::EndDisabled();
    if (create) return ComponentAction{ComponentActionKind::kCreate, name_.data()};
    if (selection.size() == 1) {
        const auto node =
                std::find_if(document.nodes_.begin(), document.nodes_.end(),
                             [&](const auto& value) { return value.id_ == selection.front(); });
        if (node != document.nodes_.end() &&
            std::any_of(document.components_.begin(), document.components_.end(),
                        [&](const auto& value) { return value.type_ == node->type_; })) {
            if (ImGui::Button((label("component.unpack") + "###component.unpack").c_str()))
                return ComponentAction{ComponentActionKind::kUnpack};
            ImGui::SameLine();
            if (ImGui::Button(label("component.detach").c_str()))
                return ComponentAction{ComponentActionKind::kDetach};
        }
    }
    if (!document.components_.empty()) {
        ImGui::TextWrapped("%s", label("component.expand_help").c_str());
        if (ImGui::Button((label("component.expand") + "###component.expand").c_str()))
            return ComponentAction{ComponentActionKind::kExpand};
        ImGui::SeparatorText(label("component.library").c_str());
    }
    for (const auto& definition : document.components_) {
        ImGui::PushID(definition.type_.c_str());
        auto title = label(definition.type_);
        if (title == definition.type_ && !definition.title_.empty()) title = definition.title_;
        ImGui::TextUnformatted(title.c_str());
        ImGui::SameLine();
        const bool add = ImGui::SmallButton(label("component.add").c_str());
        ImGui::SameLine();
        const bool edit = ImGui::SmallButton(label("component.edit").c_str());
        ImGui::PopID();
        if (add) return ComponentAction{ComponentActionKind::kAdd, definition.type_};
        if (edit) return ComponentAction{ComponentActionKind::kEdit, definition.type_};
    }
    return {};
}
editor::EditResult ExecuteComponentAction(const ComponentAction& action,
                                          const editor::Snapshot& snapshot,
                                          const graph::Registry& registry,
                                          std::span<const graph::NodeId> selection,
                                          graph::NodeId fresh_id, editor::Position insertion) {
    switch (action.kind_) {
        case ComponentActionKind::kCreate:
            return editor::MakeComponent(snapshot, registry, selection, fresh_id, action.value_);
        case ComponentActionKind::kAdd:
            return editor::AddNode(snapshot, registry, action.value_, insertion, fresh_id);
        case ComponentActionKind::kExpand:
            return editor::ExpandAllComponents(snapshot, registry, fresh_id);
        case ComponentActionKind::kEdit:
            break;
        case ComponentActionKind::kDetach:
            if (selection.size() == 1)
                return editor::DetachComponent(snapshot, registry, selection.front(),
                                               "component.user." + std::to_string(fresh_id));
            break;
        case ComponentActionKind::kUnpack:
            if (selection.size() == 1)
                return editor::UnpackComponent(snapshot, registry, selection.front(), fresh_id);
            break;
    }
    return graph::Diagnostic{"graph.component"};
}
}  // namespace rhythm::studio
