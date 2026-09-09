#pragma once

#include <cstddef>
#include <optional>
#include <string>

#include "rhythm/scene/camera.h"
#include "rhythm/scene/scene.h"

namespace rhythm::scene {
// Clip segment in world space. Screen coordinates are normalized, +Y down.
struct PickRay {
    Vector3 near_{};
    Vector3 far_{};
};
std::optional<PickRay> CameraRay(const Camera& camera, double aspect, double x, double y);
struct PickBudget {
    std::size_t instances_ = 16384;
    std::size_t nodes_ = 65536;
    std::size_t vertices_ = 1000000;
    std::size_t triangles_ = 250000;
};
struct PickHit {
    InstanceOrigin origin_{};
    NodeId model_node_ = 0;
    std::size_t instance_ = 0;
    Vector3 position_{};
    double fraction_ = 0;
};
struct PickResult {
    std::optional<PickHit> hit_{};
    std::string error_{};
    std::size_t triangles_ = 0;
};
// Synchronous geometry selection on an immutable accepted frame. Includes model
// node animation and culling; not texture-alpha/fragment visibility selection.
// Unsupported deformation or exhausted work budgets return no partial hit.
PickResult PickScene(const Scene& scene, const PickRay& ray, PickBudget budget = {});
}  // namespace rhythm::scene
