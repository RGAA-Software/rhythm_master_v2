#include "scene_canvas.h"

#include <imgui.h>

#include <array>
#include <cmath>

namespace rhythm::studio {
bool SceneCanvas::Cancel() {
    const auto active = Active();
    gizmo_.Cancel();
    edit_.reset();
    captured_viewport_.reset();
    return active;
}
OutputEdit SceneCanvas::Draw(const editor::Snapshot& snapshot, graph::NodeId selected,
                             std::uint64_t texture, geometry2d::Size extent, bool editable,
                             bool current_output, bool& enabled,
                             const std::map<std::string, std::string>& text,
                             std::span<const runtime::NodeOutput> outputs) {
    OutputEdit result;
    if (selected_ != selected || revision_ != snapshot.document_.revision_) error_.clear();
    selected_ = selected;
    revision_ = snapshot.document_.revision_;
    if ((error_ == "canvas.wait_output" && current_output) ||
        (error_ == "canvas.other_edit" && editable))
        error_.clear();
    const auto message = [&](const std::string& key) -> std::string {
        const auto found = text.find(key);
        return found == text.end() ? key : found->second;
    };
    const auto label = [&](const std::string& key) { return message(key) + "###" + key; };
    ImGui::Checkbox(label("canvas.edit").c_str(), &enabled);
    if (enabled) {
        ImGui::SameLine();
        ImGui::Checkbox(label("canvas.snap").c_str(), &snap_);
        const std::array modes{GizmoOperation::kTranslate, GizmoOperation::kRotate,
                               GizmoOperation::kScale};
        const std::array keys{"canvas.move", "canvas.rotate", "canvas.scale"};
        for (std::size_t index = 0; index < modes.size(); ++index) {
            if (index) ImGui::SameLine();
            if (ImGui::RadioButton(label(keys[index]).c_str(), mode_ == modes[index])) {
                result.preview_changed_ |= Cancel();
                mode_ = modes[index];
                if (mode_ == GizmoOperation::kScale) world_ = false;
            }
        }
        ImGui::BeginDisabled(mode_ == GizmoOperation::kScale);
        if (ImGui::Checkbox(label("scene_edit.world").c_str(), &world_))
            result.preview_changed_ |= Cancel();
        ImGui::EndDisabled();
        if (mode_ == GizmoOperation::kScale &&
            ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("%s", message("gizmo.world_scale_unsupported").c_str());
    }
    if (Active() && (!enabled || !editable || selected != edit_->Target().node_ ||
                     snapshot.document_.id_ != Preview().document_.id_ ||
                     snapshot.document_.revision_ != Preview().document_.revision_ ||
                     ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::GetIO().AppFocusLost ||
                     !ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)))
        result.preview_changed_ |= Cancel();
    std::optional<editor::SceneTarget> target;
    if (enabled && editable && (current_output || Active())) {
        const auto inspected =
                editor::InspectSceneTarget(Active() ? Preview() : snapshot, selected);
        if (std::holds_alternative<editor::SceneTarget>(inspected)) {
            target = std::get<editor::SceneTarget>(inspected);
        } else {
            result.preview_changed_ |= Cancel();
            error_ = std::get<graph::Diagnostic>(inspected).code_;
        }
    } else if (enabled) {
        error_ = editable ? "canvas.wait_output" : "canvas.other_edit";
    }
    // Reserve one clipped line while editing, so a diagnostic cannot move the
    // image under a captured mouse. Full messages remain available as tooltips.
    if (enabled) {
        ImGui::TextUnformatted(error_.empty() ? " " : message(error_).c_str());
        if (!error_.empty() && ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", message(error_).c_str());
    }
    const auto origin = ImGui::GetCursorScreenPos();
    const auto available = ImGui::GetContentRegionAvail();
    if (available.x < 1 || available.y < 1) {
        result.preview_changed_ |= Cancel();
        return result;
    }
    const auto rect = geometry2d::AspectFit(extent, {origin.x, origin.y, available.x, available.y});
    ImGui::SetCursorScreenPos({float(rect.x_), float(rect.y_)});
    ImGui::Image(texture, {float(rect.width_), float(rect.height_)});
    const auto pick = [&] {
        if (!enabled || !editable || !current_output || Active() || !ImGui::IsItemHovered() ||
            !ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            return;
        const auto mouse = ImGui::GetMousePos();
        const auto selection = PickSceneOutput(
                snapshot.document_, outputs, extent.width_ / extent.height_,
                (mouse.x - rect.x_) / rect.width_, (mouse.y - rect.y_) / rect.height_);
        if (selection.selected_) {
            result.selected_ = selection.selected_;
            error_.clear();
        } else if (!selection.error_.empty()) {
            error_ = selection.error_;
        }
    };
    if (!target) {
        pick();
        return result;
    }
    if (captured_viewport_ && (std::abs(rect.x_ - captured_viewport_->x_) > .01 ||
                               std::abs(rect.y_ - captured_viewport_->y_) > .01 ||
                               std::abs(rect.width_ - captured_viewport_->width_) > .01 ||
                               std::abs(rect.height_ - captured_viewport_->height_) > .01)) {
        result.preview_changed_ |= Cancel();
        return result;
    }
    GizmoInput input;
    input.identity_ = selected;
    input.local_ = scene::ComposeEuler(target->pose_);
    input.parent_ = target->parent_;
    input.camera_ = target->camera_;
    input.viewport_ = rect;
    input.operation_ = mode_;
    input.world_ = world_;
    input.snap_ = snap_ ? (mode_ == GizmoOperation::kRotate ? 15 : .1) : 0;
    try {
        const auto interaction = gizmo_.Draw(input);
        if (!interaction.active_ && !interaction.hovered_ && !interaction.canceled_) pick();
        if (interaction.canceled_) {
            result.preview_changed_ |= Cancel();
            return result;
        }
        if (interaction.active_ && !Active()) {
            edit_.emplace(snapshot, selected);
            captured_viewport_ = rect;
            error_.clear();
        }
        if (Active() && interaction.changed_) {
            result.preview_changed_ |= edit_->Update(interaction.local_);
            error_ = edit_->Error();
        }
        if (Active() && !interaction.active_) {
            auto finished = edit_->Finish(snapshot);
            if (std::holds_alternative<editor::Snapshot>(finished)) {
                if (std::get<editor::Snapshot>(finished).document_ != snapshot.document_)
                    result.committed_ = std::get<editor::Snapshot>(std::move(finished));
            } else {
                error_ = std::get<graph::Diagnostic>(finished).code_;
            }
            result.preview_changed_ |= Cancel();
        }
    } catch (const std::exception& error) {
        error_ = error.what();
        result.preview_changed_ |= Cancel();
    }
    return result;
}
}  // namespace rhythm::studio
