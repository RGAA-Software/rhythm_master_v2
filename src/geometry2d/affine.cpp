#include "rhythm/geometry2d/affine.h"

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>

namespace rhythm::geometry2d {
namespace {
bool Finite(Point point) { return std::isfinite(point.x_) && std::isfinite(point.y_); }
void Check(Size size) {
    if (!std::isfinite(size.width_) || !std::isfinite(size.height_) || size.width_ <= 0 ||
        size.height_ <= 0)
        throw std::invalid_argument("geometry2d.canvas");
}
bool Valid(const Affine& value) {
    return std::all_of(value.values_.begin(), value.values_.end(),
                       [](double item) { return std::isfinite(item); });
}
glm::dmat3 Native(const Affine& value) {
    if (!Valid(value)) throw std::invalid_argument("geometry2d.transform");
    const auto& a = value.values_;
    return {a[0], a[1], 0, a[2], a[3], 0, a[4], a[5], 1};
}
Affine Value(const glm::dmat3& value) {
    Affine result{{value[0][0], value[0][1], value[1][0], value[1][1], value[2][0], value[2][1]}};
    if (!Valid(result)) throw std::invalid_argument("geometry2d.transform");
    return result;
}
glm::dmat3 Translation(Point value) { return {1, 0, 0, 0, 1, 0, value.x_, value.y_, 1}; }
}  // namespace
Affine Compose(const Pose& pose, Size canvas) {
    Check(canvas);
    if (!Finite(pose.translation_) || !Finite(pose.pivot_) || !Finite(pose.scale_) ||
        !std::isfinite(pose.degrees_))
        throw std::invalid_argument("geometry2d.pose");
    const Point pivot{pose.pivot_.x_ * canvas.width_, pose.pivot_.y_ * canvas.height_};
    const glm::dmat3 rotation(
            glm::rotate(glm::dmat4(1), glm::radians(pose.degrees_), glm::dvec3(0, 0, 1)));
    const glm::dmat3 scale{pose.scale_.x_, 0, 0, 0, pose.scale_.y_, 0, 0, 0, 1};
    return Value(Translation({pivot.x_ + pose.translation_.x_ * canvas.width_,
                              pivot.y_ + pose.translation_.y_ * canvas.height_}) *
                 rotation * scale * Translation({-pivot.x_, -pivot.y_}));
}
Affine Multiply(const Affine& parent, const Affine& local) {
    return Value(Native(parent) * Native(local));
}
std::optional<Affine> Inverse(const Affine& transform) {
    if (!Valid(transform)) return {};
    const auto matrix = Native(transform);
    const double basis = std::max({std::abs(matrix[0][0]), std::abs(matrix[0][1]),
                                   std::abs(matrix[1][0]), std::abs(matrix[1][1])});
    const auto determinant = glm::determinant(matrix);
    if (!std::isfinite(determinant) || basis == 0 || std::abs(determinant) <= 1e-12 * basis * basis)
        return {};
    const auto native = glm::inverse(matrix);
    Affine result{
            {native[0][0], native[0][1], native[1][0], native[1][1], native[2][0], native[2][1]}};
    return Valid(result) ? std::optional(result) : std::nullopt;
}
Point Transform(const Affine& transform, Point point) {
    if (!Finite(point)) throw std::invalid_argument("geometry2d.point");
    const auto result = Native(transform) * glm::dvec3(point.x_, point.y_, 1);
    if (!std::isfinite(result.x) || !std::isfinite(result.y))
        throw std::invalid_argument("geometry2d.point");
    return {result.x, result.y};
}
Pose ChangePivot(const Pose& pose, Point pivot, Size canvas) {
    const auto before = Compose(pose, canvas);
    auto result = pose;
    result.pivot_ = pivot;
    const auto after = Compose(result, canvas);
    result.translation_.x_ += (before.values_[4] - after.values_[4]) / canvas.width_;
    result.translation_.y_ += (before.values_[5] - after.values_[5]) / canvas.height_;
    (void)Compose(result, canvas);
    return result;
}
Rect AspectFit(Size canvas, Rect viewport) {
    Check(canvas);
    Check({viewport.width_, viewport.height_});
    if (!std::isfinite(viewport.x_) || !std::isfinite(viewport.y_))
        throw std::invalid_argument("geometry2d.viewport");
    const auto scale = std::min(viewport.width_ / canvas.width_, viewport.height_ / canvas.height_);
    const auto width = canvas.width_ * scale, height = canvas.height_ * scale;
    return {viewport.x_ + (viewport.width_ - width) * 0.5,
            viewport.y_ + (viewport.height_ - height) * 0.5, width, height};
}
Point CanvasToScreen(Point point, Size canvas, Rect viewport) {
    if (!Finite(point)) throw std::invalid_argument("geometry2d.point");
    const auto fit = AspectFit(canvas, viewport);
    return {fit.x_ + point.x_ * fit.width_ / canvas.width_,
            fit.y_ + point.y_ * fit.height_ / canvas.height_};
}
std::optional<Point> ScreenToCanvas(Point point, Size canvas, Rect viewport, bool captured) {
    if (!Finite(point)) return {};
    const auto fit = AspectFit(canvas, viewport);
    if (!captured && (point.x_ < fit.x_ || point.y_ < fit.y_ || point.x_ > fit.x_ + fit.width_ ||
                      point.y_ > fit.y_ + fit.height_))
        return {};
    return Point{(point.x_ - fit.x_) * canvas.width_ / fit.width_,
                 (point.y_ - fit.y_) * canvas.height_ / fit.height_};
}
}  // namespace rhythm::geometry2d
