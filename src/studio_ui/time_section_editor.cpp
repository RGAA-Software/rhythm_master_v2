#include "time_section_editor.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace rhythm::studio {
void TimeSectionEditor::Reset() {
    drag_.reset();
    property_active_ = false;
}
TimeSectionEdit TimeSectionEditor::Draw(const graph::Document& document, double duration,
                                        bool can_add,
                                        const std::map<std::string, std::string>& text,
                                        const std::string& action) {
    TimeSectionEdit edit;
    if (document_ != document.id_ || revision_ != document.revision_) {
        if (document_ != document.id_) selected_ = 0;
        Reset();
        document_ = document.id_;
        revision_ = document.revision_;
    }
    std::vector<graph::NodeId> sections;
    for (const auto& node : document.nodes_)
        if (node.type_ == "time.envelope") sections.push_back(node.id_);
    ImGui::BeginDisabled(!can_add);
    edit.add_ = ImGui::Button((text.at(action) + "###timeline.add_section").c_str());
    ImGui::EndDisabled();
    if (sections.empty()) return edit;
    ImGui::TextWrapped("%s", text.at("timeline.section_help").c_str());
    if (std::find(sections.begin(), sections.end(), selected_) == sections.end())
        selected_ = sections.front();
    const auto find = [&](graph::NodeId id) -> const graph::Node& {
        return *std::find_if(document.nodes_.begin(), document.nodes_.end(),
                             [&](const auto& node) { return node.id_ == id; });
    };
    const auto range = std::clamp(std::isfinite(duration) ? duration : 10.0, 0.01, 86400.0);
    const float row_height = 32 + ImGui::GetStyle().ItemSpacing.y;
    const float padding =
            2 * (ImGui::GetStyle().WindowPadding.y + ImGui::GetStyle().ChildBorderSize);
    ImGui::BeginDisabled(property_active_);
    if (ImGui::BeginChild("###timeline.sections",
                          {0, std::min(180.0F, padding + sections.size() * row_height)},
                          ImGuiChildFlags_Borders)) {
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(sections.size()), row_height);
        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                const auto& node = find(sections[static_cast<std::size_t>(row)]);
                ImGui::PushID(std::to_string(node.id_).c_str());
                const auto origin = ImGui::GetCursorScreenPos();
                const float width = std::max(1.0F, ImGui::GetContentRegionAvail().x);
                ImGui::InvisibleButton("###section.bar", {width, 32});
                const auto start = graph::Scalar(node, "clip_start", 0);
                const auto length = graph::Scalar(node, "clip_duration", 4);
                const auto x = [&](double time) {
                    return origin.x +
                           static_cast<float>(std::clamp(time / range, 0.0, 1.0)) * width;
                };
                const float left = x(start), right = x(start + length);
                if (ImGui::IsItemActivated()) {
                    selected_ = node.id_;
                    const float mouse = ImGui::GetIO().MousePos.x;
                    if (mouse >= left - 5 && mouse <= right + 5) {
                        const int mode = right - left < 12              ? 0
                                         : std::abs(mouse - left) <= 6  ? -1
                                         : std::abs(mouse - right) <= 6 ? 1
                                                                        : 0;
                        drag_ = Drag{node, mouse, mode};
                    }
                }
                if (drag_ && drag_->origin_.id_ == node.id_) {
                    if (ImGui::IsItemActive()) {
                        const auto delta =
                                (ImGui::GetIO().MousePos.x - drag_->mouse_x_) / width * range;
                        const auto initial_start = graph::Scalar(drag_->origin_, "clip_start", 0);
                        const auto initial_length =
                                graph::Scalar(drag_->origin_, "clip_duration", 4);
                        auto changed = drag_->origin_;
                        if (drag_->mode_ == 0)
                            changed.properties_["clip_start"] =
                                    std::clamp(initial_start + delta, 0.0, 86400.0);
                        else if (drag_->mode_ < 0) {
                            const auto next = std::clamp(
                                    initial_start + delta,
                                    std::max(0.0, initial_start + initial_length - 86400),
                                    std::min(86400.0, initial_start + initial_length - 0.001));
                            changed.properties_["clip_start"] = next;
                            changed.properties_["clip_duration"] =
                                    initial_start + initial_length - next;
                        } else {
                            changed.properties_["clip_duration"] =
                                    std::clamp(initial_length + delta, 0.001, 86400.0);
                        }
                        if (changed != node) {
                            edit.changed_ = std::move(changed);
                            drag_->changed_ = true;
                        }
                    }
                    if (ImGui::IsItemDeactivated()) {
                        edit.committed_ = drag_->changed_;
                        drag_.reset();
                    }
                }
                if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                auto& draw = *ImGui::GetWindowDrawList();
                draw.AddRectFilled(origin, {origin.x + width, origin.y + 28},
                                   ImGui::GetColorU32(ImGuiCol_FrameBg), 3);
                draw.AddRectFilled(
                        {left, origin.y + 2}, {std::max(left + 1, right), origin.y + 26},
                        ImGui::GetColorU32(selected_ == node.id_ ? ImGuiCol_SliderGrabActive
                                                                 : ImGuiCol_SliderGrab),
                        3);
                for (const auto edge : {left, right})
                    draw.AddLine({edge, origin.y + 4}, {edge, origin.y + 24},
                                 ImGui::GetColorU32(ImGuiCol_Text));
                const auto label = text.at("time.envelope") + " #" + std::to_string(node.id_);
                draw.AddText({origin.x + 6, origin.y + 6}, ImGui::GetColorU32(ImGuiCol_Text),
                             label.c_str());
                ImGui::PopID();
            }
        }
    }
    ImGui::EndChild();
    ImGui::EndDisabled();
    // A dragged row can scroll outside the clipper's visible range. Finish the
    // gesture even when that row no longer submits its ImGui item this frame.
    if (drag_ && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        edit.committed_ = drag_->changed_;
        drag_.reset();
    }
    if (edit.changed_ || edit.committed_) return edit;
    auto node = find(selected_);
    static const auto kDescriptor = graph::Registry{}.Find("time.envelope").value();
    ImGui::PushID("section.properties");
    ImGui::PushID(std::to_string(selected_).c_str());
    ImGui::BeginDisabled(drag_.has_value());
    property_active_ = false;
    for (const auto& property : kDescriptor.properties_) {
        auto value = graph::Scalar(node, property.key_, std::get<double>(property.default_));
        ImGui::SetNextItemWidth(140);
        bool changed = false;
        if (property.choices_.empty()) {
            changed = ImGui::DragScalar(text.at(property.key_).c_str(), ImGuiDataType_Double,
                                        &value, 0.02F, &property.minimum_, &property.maximum_,
                                        "%.3f", ImGuiSliderFlags_AlwaysClamp);
            edit.committed_ |= ImGui::IsItemDeactivatedAfterEdit();
        } else if (ImGui::BeginCombo(text.at(property.key_).c_str(),
                                     text.at(property.choices_.at(static_cast<std::size_t>(value)))
                                             .c_str())) {
            for (std::size_t index = 0; index < property.choices_.size(); ++index)
                if (ImGui::Selectable(text.at(property.choices_[index]).c_str(), value == index)) {
                    value = static_cast<double>(index);
                    changed = edit.committed_ = true;
                }
            ImGui::EndCombo();
        }
        if (changed) {
            node.properties_[property.key_] =
                    std::isfinite(value) ? std::clamp(value, property.minimum_, property.maximum_)
                                         : std::get<double>(property.default_);
            edit.changed_ = node;
        }
        property_active_ |= ImGui::IsItemActive();
    }
    ImGui::EndDisabled();
    ImGui::PopID();
    ImGui::PopID();
    return edit;
}
}  // namespace rhythm::studio
