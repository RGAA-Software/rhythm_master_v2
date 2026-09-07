#pragma once

#include "rhythm/scene/math.h"

namespace rhythm::scene {
enum class ProjectionKind { kPerspective, kOrthographic };
struct Camera {
    Vector3 eye_{0, 0, 3};
    Vector3 target_{};
    Vector3 up_{0, 1, 0};
    ProjectionKind kind_ = ProjectionKind::kPerspective;
    double vertical_fov_ = 60;
    double orthographic_height_ = 2;
    double near_ = 0.05;
    double far_ = 1000;
};
Matrix View(const Camera& camera);
// Canonical OpenGL clip depth [-1,+1]. Backends explicitly adapt depth range
// and render-target Y orientation; domain matrices never contain native flags.
Matrix Projection(const Camera& camera, double aspect);
}  // namespace rhythm::scene
