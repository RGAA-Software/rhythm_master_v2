#pragma once

#include <cstdint>

#include "rhythm/geometry2d/affine.h"
#include "rhythm/scene/camera.h"

namespace rhythm::studio {
enum class GizmoOperation { kTranslate, kRotate, kScale };
struct GizmoInput {
    std::uint64_t identity_ = 0;
    scene::Matrix local_{};
    scene::Matrix parent_{};
    scene::Camera camera_{};
    geometry2d::Rect viewport_{};
    GizmoOperation operation_ = GizmoOperation::kTranslate;
    // World axes are supported for translation/rotation. Scale is local-only;
    // requesting world scale rejects instead of silently using different axes.
    bool world_ = false;
    bool enabled_ = true;
    double snap_ = 0;
};
struct GizmoResult {
    scene::Matrix local_{};
    bool changed_ = false;
    bool active_ = false;
    bool hovered_ = false;
    bool canceled_ = false;
};
// One UI-thread viewport capture. ImGuizmo globals and ImGui borrows remain
// private; authoring transactions receive project matrices and stable identity.
class Gizmo final {
   public:
    GizmoResult Draw(const GizmoInput& input);
    bool Cancel();

   private:
    std::uint64_t identity_ = 0;
    bool active_ = false;
    GizmoOperation operation_ = GizmoOperation::kTranslate;
    bool world_ = false;
};
}  // namespace rhythm::studio
