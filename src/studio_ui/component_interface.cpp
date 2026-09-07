#include "component_interface.h"

#include <imgui.h>

#include <algorithm>

namespace rhythm::studio {
std::optional<graph::ComponentDefinition> ComponentInterface::Draw(
        const graph::ComponentDefinition& definition, const graph::Document& body,
        graph::NodeId selected, const graph::Registry& registry,
        const std::map<std::string, std::string>& text) {
    const auto label = [&](const std::string& key) {
        const auto found = text.find(key);
        return found == text.end() ? key : found->second;
    };
    if (!ImGui::CollapsingHeader(label("component.interface").c_str(),
                                 ImGuiTreeNodeFlags_DefaultOpen))
        return {};
    auto next = definition;
    bool changed = false;
    ImGui::TextWrapped("%s", label("component.interface_help").c_str());
    ImGui::SeparatorText(label("component.public_inputs").c_str());
    for (std::size_t index = 0; index < next.inputs_.size(); ++index) {
        ImGui::PushID(static_cast<int>(index));
        const auto& port = next.inputs_[index];
        ImGui::Text("%s: %llu / %s", port.key_.c_str(), static_cast<unsigned long long>(port.node_),
                    label(port.input_).c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton(label("component.hide").c_str())) {
            next.inputs_.erase(next.inputs_.begin() + index);
            changed = true;
        } else if (index > 0) {
            ImGui::SameLine();
            if (ImGui::SmallButton(label("component.move_up").c_str())) {
                std::swap(next.inputs_[index], next.inputs_[index - 1]);
                changed = true;
            }
        }
        ImGui::PopID();
        if (changed) break;
    }
    ImGui::SeparatorText(label("component.public_parameters").c_str());
    for (std::size_t index = 0; index < next.parameters_.size() && !changed; ++index) {
        auto& parameter = next.parameters_[index];
        ImGui::PushID(static_cast<int>(index) + 256);
        ImGui::Text("%s: %llu / %s", parameter.key_.c_str(),
                    static_cast<unsigned long long>(parameter.node_),
                    label(parameter.property_).c_str());
        ImGui::TextDisabled("%s", label(parameter.group_).c_str());
        if (ImGui::SmallButton(label("component.hide").c_str())) {
            next.parameters_.erase(next.parameters_.begin() + index);
            changed = true;
        } else {
            if (index > 0) {
                ImGui::SameLine();
                if (ImGui::SmallButton(label("component.move_up").c_str())) {
                    std::swap(next.parameters_[index], next.parameters_[index - 1]);
                    changed = true;
                }
            }
            if (!changed && ImGui::SmallButton(label("component.assign_group").c_str())) {
                parameter.group_ = group_.data();
                changed = true;
            }
            if (!changed && (parameter.minimum_ || parameter.maximum_)) {
                auto minimum = parameter.minimum_.value_or(-1000000);
                auto maximum = parameter.maximum_.value_or(1000000);
                ImGui::SetNextItemWidth(110);
                bool range_changed =
                        ImGui::InputDouble(label("component.minimum").c_str(), &minimum);
                ImGui::SetNextItemWidth(110);
                range_changed |= ImGui::InputDouble(label("component.maximum").c_str(), &maximum);
                if (range_changed) {
                    parameter.minimum_ = minimum;
                    parameter.maximum_ = maximum;
                    changed = true;
                }
            }
        }
        ImGui::PopID();
    }
    ImGui::InputText(label("component.public_name").c_str(), name_.data(), name_.size());
    ImGui::InputText(label("component.group").c_str(), group_.data(), group_.size());
    const auto node = std::find_if(body.nodes_.begin(), body.nodes_.end(),
                                   [&](const auto& value) { return value.id_ == selected; });
    if (node != body.nodes_.end()) {
        const auto descriptor = registry.Find(node->type_, body.components_);
        if (descriptor) {
            if (ImGui::BeginCombo(label("component.expose_input").c_str(),
                                  label("component.choose").c_str())) {
                for (const auto& input : descriptor->inputs_) {
                    const bool exists = std::any_of(
                            next.inputs_.begin(), next.inputs_.end(), [&](const auto& port) {
                                return port.node_ == selected && port.input_ == input.key_;
                            });
                    if (!exists && ImGui::Selectable(label(input.key_).c_str())) {
                        next.inputs_.push_back(
                                {name_[0] ? name_.data() : input.key_, selected, input.key_});
                        changed = true;
                    }
                }
                ImGui::EndCombo();
            }
            if (ImGui::BeginCombo(label("component.expose_parameter").c_str(),
                                  label("component.choose").c_str())) {
                for (const auto& property : descriptor->properties_) {
                    const bool exists =
                            std::any_of(next.parameters_.begin(), next.parameters_.end(),
                                        [&](const auto& parameter) {
                                            return parameter.node_ == selected &&
                                                   parameter.property_ == property.key_;
                                        });
                    if (!exists && ImGui::Selectable(label(property.key_).c_str())) {
                        graph::ComponentParameter parameter{name_[0] ? name_.data() : property.key_,
                                                            selected, property.key_, group_.data()};
                        if (std::holds_alternative<double>(property.default_)) {
                            parameter.minimum_ = property.minimum_;
                            parameter.maximum_ = property.maximum_;
                        }
                        next.parameters_.push_back(std::move(parameter));
                        changed = true;
                    }
                }
                ImGui::EndCombo();
            }
        }
    }
    return changed ? std::optional{std::move(next)} : std::nullopt;
}
}  // namespace rhythm::studio
