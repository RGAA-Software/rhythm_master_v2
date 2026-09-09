#pragma once

#include <array>
#include <optional>

namespace rhythm::geometry2d {
struct Point {
    double x_ = 0;
    double y_ = 0;
    bool operator==(const Point&) const = default;
};
struct Size {
    double width_ = 1;
    double height_ = 1;
};
struct Rect {
    double x_ = 0;
    double y_ = 0;
    double width_ = 1;
    double height_ = 1;
};
// Column vectors, screen coordinates (+Y down). Rotation is clockwise degrees;
// pivot and translation are fractions of the authored canvas, scale is signed.
struct Pose {
    Point translation_{};
    Point pivot_{0.5, 0.5};
    Point scale_{1, 1};
    double degrees_ = 0;
};
struct Affine {
    // x' = a*x + c*y + tx, y' = b*x + d*y + ty.
    std::array<double, 6> values_{1, 0, 0, 1, 0, 0};
};
Affine Compose(const Pose& pose, Size canvas);
Affine Multiply(const Affine& parent, const Affine& local);
std::optional<Affine> Inverse(const Affine& transform);
Point Transform(const Affine& transform, Point point);
// Change an author's pivot without moving the rendered image.
Pose ChangePivot(const Pose& pose, Point pivot, Size canvas);
Rect AspectFit(Size canvas, Rect viewport);
Point CanvasToScreen(Point canvas_point, Size canvas, Rect viewport);
// A drag may continue outside after capture. Initial hits reject letterboxes.
std::optional<Point> ScreenToCanvas(Point screen_point, Size canvas, Rect viewport,
                                    bool captured = false);
}  // namespace rhythm::geometry2d
