#include "gizmo.h"

// ImGuizmo's upstream header requires ImGui declarations before inclusion.
// clang-format off
#include <imgui.h>
#include <ImGuizmo.h>
// clang-format on

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace rhythm::studio {
namespace {
std::array<float, 16> Native(const scene::Matrix& matrix) {
    std::array<float, 16> result{};
    for (std::size_t index = 0; index < result.size(); ++index) {
        const auto value = matrix.values_[index];
        if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max())
            throw std::invalid_argument("gizmo.nonfinite_matrix");
        result[index] = float(value);
    }
    return result;
}
ImGuizmo::OPERATION Native(GizmoOperation operation) {
    switch (operation) {
        case GizmoOperation::kTranslate:
            return ImGuizmo::TRANSLATE;
        case GizmoOperation::kRotate:
            return ImGuizmo::ROTATE;
        case GizmoOperation::kScale:
            return ImGuizmo::SCALE;
    }
    throw std::invalid_argument("gizmo.operation");
}
}  // namespace
bool Gizmo::Cancel() {
    const auto was_active = active_;
    if (active_) {
        ImGuizmo::Enable(false);
        ImGuizmo::Enable(true);
    }
    active_ = false;
    identity_ = 0;
    return was_active;
}
GizmoResult Gizmo::Draw(const GizmoInput& input) try {
    if (input.world_ && input.operation_ == GizmoOperation::kScale)
        throw std::invalid_argument("gizmo.world_scale_unsupported");
    GizmoResult result;
    result.local_ = input.local_;
    const auto& rect = input.viewport_;
    const auto& io = ImGui::GetIO();
    if (!input.enabled_ || !input.identity_ || io.AppFocusLost ||
        ImGui::IsKeyPressed(ImGuiKey_Escape) ||
        (active_ && (identity_ != input.identity_ || operation_ != input.operation_ ||
                     world_ != input.world_ ||
                     !ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)))) {
        result.canceled_ = Cancel();
        return result;
    }
    if (!std::isfinite(rect.x_) || !std::isfinite(rect.y_) || !std::isfinite(rect.width_) ||
        !std::isfinite(rect.height_) || rect.width_ < 1 || rect.height_ < 1 ||
        !std::isfinite(input.snap_) || input.snap_ < 0)
        throw std::invalid_argument("gizmo.viewport");
    const auto inverse_parent = scene::InverseAffine(input.parent_);
    (void)scene::InverseAffine(input.local_);
    auto matrix = Native(scene::Multiply(input.parent_, input.local_));
    const auto view = Native(scene::View(input.camera_));
    const auto projection = Native(scene::Projection(input.camera_, rect.width_ / rect.height_));
    const bool inside = io.MousePos.x >= rect.x_ && io.MousePos.x <= rect.x_ + rect.width_ &&
                        io.MousePos.y >= rect.y_ && io.MousePos.y <= rect.y_ + rect.height_;
    // Bound the actual viewport independently of upstream SetRect's mYMax typo.
    // A captured drag may continue outside; an outside press cannot start one.
    ImGuizmo::Enable(active_ || inside);
    ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());
    ImGuizmo::BeginFrame();
    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
    ImGuizmo::SetRect(float(rect.x_), float(rect.y_), float(rect.width_), float(rect.height_));
    ImGuizmo::SetOrthographic(input.camera_.kind_ == scene::ProjectionKind::kOrthographic);
    ImGuizmo::PushID(
            static_cast<int>(ImGui::GetID(("gizmo." + std::to_string(input.identity_)).c_str())));
    auto& draw = *ImGui::GetWindowDrawList();
    draw.PushClipRect({float(rect.x_), float(rect.y_)},
                      {float(rect.x_ + rect.width_), float(rect.y_ + rect.height_)}, true);
    const std::array snap{float(input.snap_), float(input.snap_), float(input.snap_)};
    const auto changed =
            ImGuizmo::Manipulate(view.data(), projection.data(), Native(input.operation_),
                                 input.world_ ? ImGuizmo::WORLD : ImGuizmo::LOCAL, matrix.data(),
                                 nullptr, input.snap_ > 0 ? snap.data() : nullptr);
    result.active_ = ImGuizmo::IsUsing();
    result.hovered_ = inside && ImGuizmo::IsOver();
    ImGuizmo::PopID();
    draw.PopClipRect();
    if (changed) {
        scene::Matrix world;
        std::copy(matrix.begin(), matrix.end(), world.values_.begin());
        result.local_ = scene::Multiply(inverse_parent, world);
        if (!scene::ValidAffine(result.local_)) throw std::invalid_argument("gizmo.result");
        result.changed_ = true;
    }
    active_ = result.active_;
    identity_ = input.identity_;
    operation_ = input.operation_;
    world_ = input.world_;
    if (result.hovered_ || active_) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    return result;
} catch (...) {
    Cancel();
    throw;
}
}  // namespace rhythm::studio
