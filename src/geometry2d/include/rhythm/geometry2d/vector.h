#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "rhythm/geometry2d/affine.h"

namespace rhythm::geometry2d {
inline constexpr std::size_t kMaximumVectorPoints = 1024;
inline constexpr std::size_t kMaximumVectorVertices = 16384;
struct Contour {
    std::vector<Point> points_{};
    bool closed_ = false;
};
struct VectorMesh {
    std::vector<Point> vertices_{};
    std::vector<std::uint32_t> indices_{};
    bool operator==(const VectorMesh&) const = default;
};
enum class LineJoin { kMiter, kBevel, kRound };
enum class LineCap { kButt, kSquare, kRound };
struct StrokeStyle {
    double width_ = 2;
    double miter_limit_ = 4;
    double arc_tolerance_ = 0.01;
    LineJoin join_ = LineJoin::kRound;
    LineCap cap_ = LineCap::kRound;
};
// Canvas coordinates (+Y down), rounded to 1e-6 units; finite coordinates in
// [-10000,10000]. At most 64 contours / 1024 input points. No GPU resources.
// Fill uses even-odd winding, including holes and self intersections. Only
// closed contours are accepted. Degenerate/empty contours yield empty regions.
VectorMesh FillContours(std::span<const Contour> contours);
// Closed paths join their ends; open paths use the explicit cap. Width zero is
// empty. Output is bounded to 16384 vertices and 49152 indices in either path.
VectorMesh StrokeContours(std::span<const Contour> contours, const StrokeStyle& style = {});
}  // namespace rhythm::geometry2d
