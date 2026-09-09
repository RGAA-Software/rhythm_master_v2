#include "output_canvas.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace rhythm::studio {
namespace {
using geometry2d::Point;
ImVec2 Native(Point point) { return {float(point.x_), float(point.y_)}; }
double Distance(Point a, Point b) { return std::hypot(a.x_ - b.x_, a.y_ - b.y_); }
}  // namespace
bool OutputCanvas::Cancel() {
    const auto active = Active();
    scene_.Cancel();
    edit_.reset();
    captured_viewport_.reset();
    return active;
}
OutputEdit OutputCanvas::Draw(const editor::Snapshot& snapshot, graph::NodeId selected,
                              std::uint64_t texture, geometry2d::Size extent, bool editable,
                              bool current_output, const std::map<std::string, std::string>& text) {
    OutputEdit result;
    const auto& nodes = snapshot.document_.nodes_;
    const auto scene_node = [&](graph::NodeId id, const std::string& type) {
        return std::any_of(nodes.begin(), nodes.end(),
                           [&](const auto& node) { return node.id_ == id && node.type_ == type; });
    };
    const auto direct_scene = std::any_of(snapshot.document_.edges_.begin(),
                                          snapshot.document_.edges_.end(), [&](const auto& edge) {
                                              return edge.to_ == snapshot.document_.output_ &&
                                                     edge.input_ == "source" &&
                                                     scene_node(edge.from_, "scene.render");
                                          });
    if (direct_scene || scene_node(selected, "scene.transform")) {
        const auto canceled = edit_.has_value();
        edit_.reset();
        captured_viewport_.reset();
        auto scene_result = scene_.Draw(snapshot, selected, texture, extent, editable,
                                        current_output, enabled_, text);
        scene_result.preview_changed_ |= canceled;
        return scene_result;
    }
    result.preview_changed_ = scene_.Cancel();
    const auto label = [&](const std::string& key) {
        const auto found = text.find(key);
        return (found == text.end() ? key : found->second) + "###" + key;
    };
    const auto hint = [&](const std::string& key) {
        const auto found = text.find(key);
        ImGui::TextWrapped("%s", (found == text.end() ? key : found->second).c_str());
    };
    ImGui::Checkbox(label("canvas.edit").c_str(), &enabled_);
    if (enabled_) {
        ImGui::SameLine();
        ImGui::Checkbox(label("canvas.snap").c_str(), &snap_);
        const std::array modes{editor::CanvasGesture::kTranslate, editor::CanvasGesture::kRotate,
                               editor::CanvasGesture::kScale, editor::CanvasGesture::kPivot};
        const std::array keys{"canvas.move", "canvas.rotate", "canvas.scale", "canvas.pivot"};
        for (std::size_t index = 0; index < modes.size(); ++index) {
            if (index) ImGui::SameLine();
            if (ImGui::RadioButton(label(keys[index]).c_str(), mode_ == modes[index])) {
                result.preview_changed_ |= Cancel();
                mode_ = modes[index];
            }
        }
    }
    if (Active() && (!enabled_ || !editable || selected != edit_->Target().node_ ||
                     snapshot.document_.id_ != Preview().document_.id_ ||
                     snapshot.document_.revision_ != Preview().document_.revision_ ||
                     ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::GetIO().AppFocusLost ||
                     !ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)))
        result.preview_changed_ |= Cancel();
    std::optional<editor::CanvasTarget> target;
    if (enabled_ && editable && (current_output || Active())) {
        const auto inspected =
                editor::InspectCanvasTarget(Active() ? Preview() : snapshot, selected);
        if (std::holds_alternative<editor::CanvasTarget>(inspected))
            target = std::get<editor::CanvasTarget>(inspected);
        else if (!Active())
            hint(std::get<graph::Diagnostic>(inspected).code_);
    } else if (enabled_) {
        hint(editable ? "canvas.wait_output" : "canvas.other_edit");
    }
    const auto origin = ImGui::GetCursorScreenPos();
    const auto available = ImGui::GetContentRegionAvail();
    if (available.x < 1 || available.y < 1) {
        result.preview_changed_ |= Cancel();
        return result;
    }
    const geometry2d::Rect viewport{origin.x, origin.y, available.x, available.y};
    if (captured_viewport_ && (std::abs(viewport.x_ - captured_viewport_->x_) > .01 ||
                               std::abs(viewport.y_ - captured_viewport_->y_) > .01 ||
                               std::abs(viewport.width_ - captured_viewport_->width_) > .01 ||
                               std::abs(viewport.height_ - captured_viewport_->height_) > .01))
        result.preview_changed_ |= Cancel();
    const auto fit = geometry2d::AspectFit(extent, viewport);
    ImGui::SetCursorScreenPos({float(fit.x_), float(fit.y_)});
    ImGui::Image(texture, {float(fit.width_), float(fit.height_)});
    if (!target && !Active()) return result;
    // An intermediate zero scale is legal during capture, but not pickable.
    // Keep the original gesture until release even while its outline is singular.
    ImGui::SetCursorScreenPos({float(fit.x_), float(fit.y_)});
    ImGui::InvisibleButton("###canvas.manipulator", {float(fit.width_), float(fit.height_)});
    const Point mouse{ImGui::GetIO().MousePos.x, ImGui::GetIO().MousePos.y};
    bool hit = false;
    if (target) {
        const auto matrix = geometry2d::Multiply(
                target->parent_, geometry2d::Compose(target->pose_, target->canvas_));
        const auto screen = [&](Point point) {
            return geometry2d::CanvasToScreen(geometry2d::Transform(matrix, point), target->canvas_,
                                              viewport);
        };
        const auto size = target->canvas_;
        const std::array corners{screen({0, 0}), screen({size.width_, 0}),
                                 screen({size.width_, size.height_}), screen({0, size.height_})};
        const auto pivot = screen(
                {target->pose_.pivot_.x_ * size.width_, target->pose_.pivot_.y_ * size.height_});
        auto& draw = *ImGui::GetWindowDrawList();
        draw.PushClipRect({float(fit.x_), float(fit.y_)},
                          {float(fit.x_ + fit.width_), float(fit.y_ + fit.height_)}, true);
        const auto color = IM_COL32(255, 207, 66, 255);
        for (std::size_t index = 0; index < corners.size(); ++index)
            draw.AddLine(Native(corners[index]), Native(corners[(index + 1) % 4]), color, 2);
        const auto canvas_point = geometry2d::ScreenToCanvas(mouse, size, viewport);
        if (mode_ == editor::CanvasGesture::kTranslate && canvas_point) {
            const auto inverse = geometry2d::Inverse(matrix);
            if (inverse) {
                const auto local = geometry2d::Transform(*inverse, *canvas_point);
                hit = local.x_ >= 0 && local.y_ >= 0 && local.x_ <= size.width_ &&
                      local.y_ <= size.height_;
            }
        } else if (mode_ == editor::CanvasGesture::kScale) {
            for (const auto corner : corners) {
                draw.AddRectFilled({float(corner.x_ - 5), float(corner.y_ - 5)},
                                   {float(corner.x_ + 5), float(corner.y_ + 5)}, color);
                hit |= Distance(mouse, corner) <= 12;
            }
        } else if (mode_ == editor::CanvasGesture::kRotate) {
            // Keep the handle inside the transformed canvas so a full-size
            // object remains rotatable without making letterboxes pickable.
            const auto handle = screen({size.width_ * .5, size.height_ * .1});
            draw.AddLine(Native(pivot), Native(handle), color);
            draw.AddCircle(Native(handle), 8, color, 0, 2);
            hit = Distance(mouse, handle) <= 12;
        } else if (mode_ == editor::CanvasGesture::kPivot) {
            hit = Distance(mouse, pivot) <= 12;
        }
        draw.AddCircle(Native(pivot), 6, color, 0, 2);
        draw.PopClipRect();
        hit &= canvas_point.has_value();
    }
    if ((hit && ImGui::IsItemHovered()) || Active())
        ImGui::SetMouseCursor(mode_ == editor::CanvasGesture::kTranslate
                                      ? ImGuiMouseCursor_ResizeAll
                                      : ImGuiMouseCursor_Hand);
    if (!Active() && target && current_output && hit && ImGui::IsItemHovered() &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        const auto point = geometry2d::ScreenToCanvas(mouse, target->canvas_, viewport);
        if (point) {
            edit_.emplace(snapshot, selected, mode_, *point);
            captured_viewport_ = viewport;
        }
    }
    if (Active()) {
        const auto point =
                geometry2d::ScreenToCanvas(mouse, edit_->Target().canvas_, viewport, true);
        if (point && (ImGui::IsMouseDown(0) || ImGui::IsMouseReleased(0)))
            result.preview_changed_ |= edit_->Update(
                    *point, snap_ ? editor::CanvasSnap{16, 15, .1} : editor::CanvasSnap{});
        if (ImGui::IsMouseReleased(0)) {
            auto finished = edit_->Finish(snapshot);
            if (std::holds_alternative<editor::Snapshot>(finished) &&
                std::get<editor::Snapshot>(finished).document_ != snapshot.document_)
                result.committed_ = std::get<editor::Snapshot>(std::move(finished));
            result.preview_changed_ |= Cancel();
        }
    }
    return result;
}
}  // namespace rhythm::studio
