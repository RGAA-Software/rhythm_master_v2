#pragma once

#include <optional>

#include "rhythm/scene/math.h"

namespace rhythm::scene {
// Intrinsic X, then Y, then Z: translation * Rz * Ry * Rx * signed scale.
// Author rotations are degrees; no backend/GLM types cross this contract.
struct EulerPose {
    Vector3 translation_{};
    Vector3 degrees_{};
    Vector3 scale_{1, 1, 1};
};
Matrix ComposeEuler(const EulerPose& pose);
// Reject singular, nonfinite and sheared matrices instead of silently changing
// their image. A reflected basis is represented by negative X scale.
std::optional<EulerPose> DecomposeEuler(const Matrix& matrix);
}  // namespace rhythm::scene
