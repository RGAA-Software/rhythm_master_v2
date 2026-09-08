#include "property_inspector.h"

#include <imgui.h>

#include <algorithm>

#include "curve_editor.h"

namespace rhythm::studio {
namespace {
std::string Text(const std::map<std::string, std::string>& text, const std::string& key) {
    const auto found = text.find(key);
    return found == text.end() ? key : found->second;
}
std::string Label(const std::map<std::string, std::string>& text, const std::string& key) {
    return Text(text, key) + "###" + key;
}
std::vector<std::string> AnimationNames(const graph::Document& document, graph::NodeId selected,
                                        const scene::Resources& models) {
    for (std::size_t depth = 0; depth < 64; ++depth) {
        const auto node =
                std::find_if(document.nodes_.begin(), document.nodes_.end(),
                             [selected](const auto& value) { return value.id_ == selected; });
        if (node == document.nodes_.end()) return {};
        if (node->type_ == "geometry.glb") {
            const auto asset = node->properties_.find("asset");
            if (asset == node->properties_.end() ||
                !std::holds_alternative<assets::AssetId>(asset->second))
                return {};
            for (const auto& model : models.models_)
                if (model.id_ == std::get<assets::AssetId>(asset->second) && model.model_) {
                    std::vector<std::string> names;
                    for (const auto& clip : model.model_->animations_)
                        names.push_back(std::to_string(names.size()) + ": " + clip.Name());
                    return names;
                }
            return {};
        }
        if (node->type_ != "geometry.animate" && node->type_ != "geometry.deform") return {};
        const auto edge = std::find_if(
                document.edges_.begin(), document.edges_.end(), [selected](const auto& value) {
                    return value.to_ == selected && value.input_ == "geometry";
                });
        if (edge == document.edges_.end()) return {};
        selected = edge->from_;
    }
    return {};
}
}  // namespace
InspectorResult PropertyInspector::Draw(const editor::Snapshot& base, graph::NodeId selected,
                                        const graph::Registry& registry,
                                        std::span<const content::Preset> presets,
                                        const std::map<std::string, std::string>& text,
                                        const std::string& locale, const scene::Resources& models) {
    InspectorResult result;

    if (draft_ && (draft_->document_.revision_ != base.document_.revision_ ||
                   draft_->document_.id_ != base.document_.id_)) {
        draft_.reset();
        result.preview_changed_ = true;
    }
    const auto& snapshot = draft_ ? *draft_ : base;
    const auto found =
            std::find_if(snapshot.document_.nodes_.begin(), snapshot.document_.nodes_.end(),
                         [&](const auto& node) { return node.id_ == selected; });
    if (found == snapshot.document_.nodes_.end()) {
        ImGui::TextUnformatted(Text(text, "select_node").c_str());
        return result;
    }
    const auto node = *found;
    const auto animation_names = node.type_ == "geometry.animate"
                                         ? AnimationNames(snapshot.document_, selected, models)
                                         : std::vector<std::string>{};
    auto title = Text(text, node.type_);
    if (title == node.type_) {
        const auto definition = std::find_if(
                snapshot.document_.components_.begin(), snapshot.document_.components_.end(),
                [&](const auto& value) { return value.type_ == node.type_; });
        if (definition != snapshot.document_.components_.end() && !definition->title_.empty())
            title = definition->title_;
    }
    ImGui::TextUnformatted(title.c_str());
    if (node.type_ == "geometry.animate")
        ImGui::TextWrapped("%s", Text(text, "animation.help").c_str());
    const auto descriptor = registry.Find(node.type_, snapshot.document_.components_);
    if (!descriptor) return result;
    if (!registry.ValidateNode(node, snapshot.document_.components_).empty()) {
        ImGui::TextWrapped("%s", Text(text, "graph.property_type").c_str());
        return result;
    }
    if (auto edited = binding_editor_.Draw(snapshot, selected, registry, text)) {
        draft_.reset();
        result.committed_ = std::move(edited);
        return result;
    }
    const bool has_presets = std::any_of(presets.begin(), presets.end(), [&](const auto& preset) {
        return preset.operator_type_ == node.type_;
    });
    if (has_presets &&
        ImGui::BeginCombo(Label(text, "preset").c_str(), Text(text, "choose_preset").c_str())) {
        for (const auto& preset : presets) {
            if (preset.operator_type_ != node.type_) continue;
            const auto label = preset.titles_.at(locale) + "###" + preset.id_;
            if (ImGui::Selectable(label.c_str())) {
                try {
                    auto next = snapshot;
                    for (auto& item : next.document_.nodes_)
                        if (item.id_ == selected)
                            item = content::ApplyPreset(item, preset, registry,
                                                        snapshot.document_.components_);
                    draft_.reset();
                    result.committed_ = std::move(next);
                } catch (const std::exception&) {
                    result.diagnostic_ = graph::Diagnostic{"content.preset_incompatible"};
                }
                ImGui::EndCombo();
                return result;
            }
        }
        ImGui::EndCombo();
    }
    std::string group;
    for (const auto& property : descriptor->properties_) {
        if (!property.group_.empty() && property.group_ != group) {
            group = property.group_;
            ImGui::SeparatorText(Text(text, group).c_str());
        }
        const auto value = node.properties_.contains(property.key_)
                                   ? node.properties_.at(property.key_)
                                   : property.default_;
        auto edited = value;
        bool changed = false;
        bool committed = false;
        const bool discrete = !property.choices_.empty();
        const auto label = Text(text, property.key_) + "###property." + std::to_string(selected) +
                           "." + property.key_;
        const bool connected =
                std::any_of(snapshot.document_.edges_.begin(), snapshot.document_.edges_.end(),
                            [&](const auto& edge) {
                                return edge.to_ == selected && edge.input_ == property.key_;
                            });
        const bool bound =
                std::any_of(snapshot.document_.bindings_.begin(),
                            snapshot.document_.bindings_.end(), [&](const auto& binding) {
                                return binding.node_ == selected && binding.input_ == property.key_;
                            });
        ImGui::BeginDisabled(connected || bound);
        if (discrete) {
            const auto current = static_cast<std::size_t>(std::get<double>(value));
            if (ImGui::BeginCombo(label.c_str(),
                                  Text(text, property.choices_.at(current)).c_str())) {
                for (std::size_t index = 0; index < property.choices_.size(); ++index) {
                    const auto choice =
                            Text(text, property.choices_[index]) + "###" + property.choices_[index];
                    if (ImGui::Selectable(choice.c_str(), current == index)) {
                        edited = static_cast<double>(index);
                        changed = true;
                    }
                }
                ImGui::EndCombo();
            }
        } else if (std::holds_alternative<assets::AssetId>(value)) {
            const auto& current = std::get<assets::AssetId>(value);
            const auto selected_label = current.sha256_.empty() ? Text(text, "asset.unassigned")
                                                                : current.sha256_.substr(0, 12);
            if (ImGui::BeginCombo(label.c_str(), selected_label.c_str())) {
                if (ImGui::Selectable(Text(text, "asset.unassigned").c_str(),
                                      current.sha256_.empty())) {
                    edited = assets::AssetId{};
                    changed = committed = true;
                }
                for (const auto& record : snapshot.assets_) {
                    const auto choice = record.media_type_ + " " +
                                        record.id_.sha256_.substr(0, 12) + "###" +
                                        record.id_.sha256_;
                    if (ImGui::Selectable(choice.c_str(), current == record.id_)) {
                        edited = record.id_;
                        changed = committed = true;
                    }
                }
                ImGui::EndCombo();
            }
        } else if (std::holds_alternative<parameters::Expression>(value)) {
            if (auto expression = expression_editor_.Draw(std::get<parameters::Expression>(value),
                                                          label, text)) {
                edited = std::move(*expression);
                changed = committed = true;
            }
        } else if (std::holds_alternative<parameters::Curve>(value)) {
            auto curve = std::get<parameters::Curve>(value);
            const auto curve_edit = DrawCurveEditor(curve, label, text);
            changed = curve_edit.changed_;
            committed = curve_edit.committed_;
            edited = std::move(curve);
        } else if (std::holds_alternative<double>(value)) {
            if (!animation_names.empty() && node.type_ == "geometry.animate" &&
                (property.key_ == "animation_clip" || property.key_ == "animation_second")) {
                const auto current = static_cast<std::size_t>(std::get<double>(value));
                const auto preview = current < animation_names.size()
                                             ? animation_names[current]
                                             : Text(text, "runtime.animation_clip");
                if (ImGui::BeginCombo(label.c_str(), preview.c_str())) {
                    for (std::size_t i = 0; i < animation_names.size(); ++i) {
                        const auto choice = animation_names[i] + "###clip." + std::to_string(i);
                        if (ImGui::Selectable(choice.c_str(), i == current)) {
                            edited = static_cast<double>(i);
                            changed = committed = true;
                        }
                    }
                    ImGui::EndCombo();
                }
            } else if (property.integral_) {
                auto number = static_cast<std::int64_t>(std::get<double>(value));
                const auto minimum = static_cast<std::int64_t>(property.minimum_);
                const auto maximum = static_cast<std::int64_t>(property.maximum_);
                changed = ImGui::DragScalar(label.c_str(), ImGuiDataType_S64, &number, 1, &minimum,
                                            &maximum, "%lld", ImGuiSliderFlags_AlwaysClamp);
                edited = static_cast<double>(std::clamp(number, minimum, maximum));
            } else {
                auto number = std::get<double>(value);
                changed = ImGui::DragScalar(label.c_str(), ImGuiDataType_Double, &number, 0.01f,
                                            &property.minimum_, &property.maximum_, "%.6g",
                                            ImGuiSliderFlags_AlwaysClamp);
                edited = std::clamp(number, property.minimum_, property.maximum_);
            }
        } else if (std::holds_alternative<graph::Color>(value)) {
            const auto color = std::get<graph::Color>(value);
            float channels[]{float(color.r_), float(color.g_), float(color.b_), float(color.a_)};
            changed = ImGui::ColorEdit4(label.c_str(), channels);
            edited = graph::Color{channels[0], channels[1], channels[2], channels[3]};
        }
        ImGui::EndDisabled();
        if (changed) {
            if (!draft_) draft_ = base;
            for (auto& item : draft_->document_.nodes_)
                if (item.id_ == selected) item.properties_[property.key_] = edited;
            result.preview_changed_ = true;
        }
        if (((discrete && changed) || committed || ImGui::IsItemDeactivatedAfterEdit()) && draft_) {
            auto next = std::move(*draft_);
            draft_.reset();
            result.committed_ = std::move(next);
            return result;
        }
        if (connected) ImGui::TextDisabled("%s", Text(text, "property.connected").c_str());
        if (bound) ImGui::TextDisabled("%s", Text(text, "property.bound").c_str());
    }

    return result;
}
}  // namespace rhythm::studio
