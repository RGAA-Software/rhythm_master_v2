#include "rhythm/editor/canvas_edit.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace rhythm::editor {
namespace {
double Snap(double value, double step) {
    return step > 0 ? std::round(value / step) * step : value;
}
geometry2d::Point Difference(geometry2d::Point first, geometry2d::Point second) {
    return {first.x_ - second.x_, first.y_ - second.y_};
}
}  // namespace
CanvasEdit::CanvasEdit(Snapshot base, graph::NodeId node, CanvasGesture gesture,
                       geometry2d::Point start)
    : draft_(std::move(base)), gesture_(gesture) {
    auto inspected = InspectCanvasTarget(draft_, node);
    if (std::holds_alternative<graph::Diagnostic>(inspected))
        throw std::invalid_argument(std::get<graph::Diagnostic>(inspected).code_);
    target_ = std::get<CanvasTarget>(inspected);
    inverse_parent_ = geometry2d::Inverse(target_.parent_).value();
    start_ = geometry2d::Transform(inverse_parent_, start);
}
bool CanvasEdit::Update(geometry2d::Point point, CanvasSnap snap) {
    if (!std::isfinite(point.x_) || !std::isfinite(point.y_) || !std::isfinite(snap.pixels_) ||
        !std::isfinite(snap.degrees_) || !std::isfinite(snap.scale_) || snap.pixels_ < 0 ||
        snap.degrees_ < 0 || snap.scale_ < 0)
        return false;
    const auto local = geometry2d::Transform(inverse_parent_, point);
    auto pose = target_.pose_;
    const auto canvas = target_.canvas_;
    const geometry2d::Point pivot{(pose.pivot_.x_ + pose.translation_.x_) * canvas.width_,
                                  (pose.pivot_.y_ + pose.translation_.y_) * canvas.height_};
    if (gesture_ == CanvasGesture::kTranslate) {
        pose.translation_.x_ += Snap(local.x_ - start_.x_, snap.pixels_) / canvas.width_;
        pose.translation_.y_ += Snap(local.y_ - start_.y_, snap.pixels_) / canvas.height_;
    } else if (gesture_ == CanvasGesture::kRotate) {
        const auto first = Difference(start_, pivot), next = Difference(local, pivot);
        if (std::hypot(first.x_, first.y_) < 1e-8 || std::hypot(next.x_, next.y_) < 1e-8)
            return false;
        const auto delta =
                std::remainder(std::atan2(next.y_, next.x_) - std::atan2(first.y_, first.x_),
                               2 * std::numbers::pi);
        pose.degrees_ = Snap(pose.degrees_ + delta * 180 / std::numbers::pi, snap.degrees_);
    } else if (gesture_ == CanvasGesture::kScale) {
        geometry2d::Pose rotation;
        rotation.pivot_ = {0, 0};
        rotation.degrees_ = -pose.degrees_;
        const auto matrix = geometry2d::Compose(rotation, canvas);
        const auto first = geometry2d::Transform(matrix, Difference(start_, pivot));
        const auto next = geometry2d::Transform(matrix, Difference(local, pivot));
        if (std::abs(first.x_) > 1e-8)
            pose.scale_.x_ = Snap(pose.scale_.x_ * next.x_ / first.x_, snap.scale_);
        if (std::abs(first.y_) > 1e-8)
            pose.scale_.y_ = Snap(pose.scale_.y_ * next.y_ / first.y_, snap.scale_);
    } else if (gesture_ == CanvasGesture::kPivot) {
        const auto inverse = geometry2d::Inverse(geometry2d::Compose(pose, canvas)).value();
        const auto source = geometry2d::Transform(inverse, local);
        pose = geometry2d::ChangePivot(
                pose,
                {std::clamp(Snap(source.x_, snap.pixels_) / canvas.width_, 0.0, 1.0),
                 std::clamp(Snap(source.y_, snap.pixels_) / canvas.height_, 0.0, 1.0)},
                canvas);
    } else {
        return false;
    }
    if (gesture_ == CanvasGesture::kPivot &&
        (std::abs(pose.translation_.x_) > 4 || std::abs(pose.translation_.y_) > 4))
        return false;
    auto& nodes = draft_.document_.nodes_;
    auto& node = *std::find_if(nodes.begin(), nodes.end(),
                               [&](const auto& value) { return value.id_ == target_.node_; });
    const std::map<std::string, double> values{
            {"translate_x", std::clamp(pose.translation_.x_, -4.0, 4.0)},
            {"translate_y", std::clamp(pose.translation_.y_, -4.0, 4.0)},
            {"rotation", std::clamp(pose.degrees_, -36000.0, 36000.0)},
            {"scale_x", std::clamp(pose.scale_.x_ / target_.uniform_scale_, -8.0, 8.0)},
            {"scale_y", std::clamp(pose.scale_.y_ / target_.uniform_scale_, -8.0, 8.0)},
            {"pivot_x", pose.pivot_.x_},
            {"pivot_y", pose.pivot_.y_}};
    bool changed = false;
    for (const auto& [key, value] : values) {
        const auto fallback = key.starts_with("scale_")   ? 1.0
                              : key.starts_with("pivot_") ? 0.5
                                                          : 0.0;
        if (graph::Scalar(node, key, fallback) == value) continue;
        node.properties_[key] = value;
        changed = true;
    }
    return changed;
}
EditResult CanvasEdit::Finish(const Snapshot& current) const {
    if (current.document_.id_ != draft_.document_.id_ ||
        current.document_.revision_ != draft_.document_.revision_)
        return graph::Diagnostic{"canvas.stale_edit", target_.node_, {}};
    return draft_;
}
}  // namespace rhythm::editor
