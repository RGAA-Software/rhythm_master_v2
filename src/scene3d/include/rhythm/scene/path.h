#pragma once

#include <span>

#include "rhythm/scene/model.h"

namespace rhythm::scene {
inline constexpr std::size_t kMaximumPathPoints = 1024;
// Ordered world-space control points. A closed path omits a duplicate final point.
struct Path {
    std::vector<Vector3> points_{};
    bool closed_ = false;
};
void Validate(const Path& path);
Path Helix(std::uint32_t points, double radius, double height, double turns, double phase,
           bool closed = false);
// Uniform parameter sampling of the GLM Catmull-Rom interpolant, not arc length.
Path Resample(const Path& path, std::uint32_t points);
// TiXL extrusion topology with GLM rotation-minimizing frames, UV seam and end caps.
Model Tube(const Path& path, double radius, std::uint32_t sides = 12);
}  // namespace rhythm::scene
