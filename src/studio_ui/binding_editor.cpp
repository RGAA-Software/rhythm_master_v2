#include "binding_editor.h"

#include <imgui.h>

#include <algorithm>

#include "operator_help.h"

namespace rhythm::studio {
void BindingEditor::Reset() {
    selected_ = 0;
    name_.fill(0);
    error_.reset();
}
std::optional<editor::Snapshot> BindingEditor::Draw(
        const editor::Snapshot& snapshot, graph::NodeId selected, const graph::Registry& registry,
        const std::map<std::string, std::string>& text) {
    const auto label = [&](const std::string& key) {
        const auto found = text.find(key);
        return found == text.end() ? key : found->second;
    };
    if (selected != selected_) {
        Reset();
        selected_ = selected;
    }
    if (!ImGui::CollapsingHeader((label("binding.title") + "###binding.title").c_str())) return {};
    ImGui::TextWrapped("%s", label("binding.help").c_str());
    std::optional<editor::Snapshot> result;
    const auto apply = [&](editor::EditResult edit) {
        if (std::holds_alternative<graph::Diagnostic>(edit))
            error_ = std::get<graph::Diagnostic>(std::move(edit));
        else {
            error_.reset();
            result = std::get<editor::Snapshot>(std::move(edit));
        }
    };
    ImGui::InputText((label("binding.name") + "###binding.name").c_str(), name_.data(),
                     name_.size());
    if (ImGui::Button((label("binding.export") + "###binding.export").c_str()))
        apply(editor::DefineSignal(snapshot, registry, name_.data(), selected));
    if (result) return result;
    for (const auto& signal : snapshot.document_.signals_) {
        if (signal.source_ != selected) continue;
        ImGui::PushID(signal.name_.c_str());
        ImGui::TextUnformatted(signal.name_.c_str());
        ImGui::SameLine();
        const bool remove = ImGui::SmallButton(label("binding.remove").c_str());
        ImGui::PopID();
        if (remove) return editor::RemoveSignal(snapshot, signal.name_);
    }
    const auto node =
            std::find_if(snapshot.document_.nodes_.begin(), snapshot.document_.nodes_.end(),
                         [&](const auto& entry) { return entry.id_ == selected; });
    if (node == snapshot.document_.nodes_.end()) return {};
    const auto descriptor = registry.Find(node->type_, snapshot.document_.components_);
    if (!descriptor) return {};
    for (const auto& port : descriptor->inputs_) {
        const auto bound =
                std::find_if(snapshot.document_.bindings_.begin(),
                             snapshot.document_.bindings_.end(), [&](const auto& binding) {
                                 return binding.node_ == selected && binding.input_ == port.key_;
                             });
        const auto preview = bound == snapshot.document_.bindings_.end() ? label("binding.none")
                                                                         : bound->signal_;
        const auto input_label = label(OperatorFieldKey(descriptor->type_, port.key_)) +
                                 "###binding.input." + port.key_;
        if (ImGui::BeginCombo(input_label.c_str(), preview.c_str())) {
            if (ImGui::Selectable(label("binding.none").c_str(),
                                  bound == snapshot.document_.bindings_.end()) &&
                bound != snapshot.document_.bindings_.end())
                result = editor::UnbindInput(snapshot, selected, port.key_);
            for (const auto& signal : snapshot.document_.signals_) {
                const auto source = std::find_if(
                        snapshot.document_.nodes_.begin(), snapshot.document_.nodes_.end(),
                        [&](const auto& item) { return item.id_ == signal.source_; });
                if (source == snapshot.document_.nodes_.end()) continue;
                const auto source_type =
                        registry.Find(source->type_, snapshot.document_.components_);
                if (!source_type || source_type->output_ != port.type_) continue;
                if (ImGui::Selectable(signal.name_.c_str(), signal.name_ == preview))
                    apply(editor::BindInput(snapshot, registry, selected, port.key_, signal.name_));
            }
            ImGui::EndCombo();
        }
        if (result) return result;
    }
    if (error_) ImGui::TextWrapped("%s", label(error_->code_).c_str());
    return {};
}
}  // namespace rhythm::studio
