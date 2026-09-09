#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "rhythm/geometry2d/affine.h"

namespace {
using namespace rhythm::geometry2d;
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void Near(Point actual, Point expected) {
    Require(std::abs(actual.x_ - expected.x_) < 1e-8 && std::abs(actual.y_ - expected.y_) < 1e-8,
            "canvas transform coordinate mismatch");
}
void Run() {
    const Size canvas{200, 100};
    Pose pose;
    pose.degrees_ = 90;
    Near(Transform(Compose(pose, canvas), {0, 0}), {150, -50});
    pose.scale_ = {-2, 0.5};
    pose.translation_ = {0.25, -0.5};
    const auto original = Compose(pose, canvas);
    const auto repivoted = Compose(ChangePivot(pose, {0.1, 0.9}, canvas), canvas);
    for (const Point point : {Point{0, 0}, {200, 100}, {-40, 250}}) {
        Near(Transform(original, point), Transform(repivoted, point));
        Near(Transform(Inverse(original).value(), Transform(original, point)), point);
    }
    Pose parent;
    parent.scale_ = {0.25, -3};
    parent.degrees_ = -32;
    const auto combined = Multiply(Compose(parent, canvas), original);
    const Point child{17, 91};
    Near(Transform(combined, child),
         Transform(Compose(parent, canvas), Transform(original, child)));
    Near(Transform(Inverse(combined).value(), Transform(combined, child)), child);
    pose.scale_.x_ = 0;
    Require(!Inverse(Compose(pose, canvas)), "singular parent must disable inverse manipulation");
    Affine invalid;
    invalid.values_[0] = std::numeric_limits<double>::quiet_NaN();
    Require(!Inverse(invalid), "nonfinite transform rejected");
    for (const auto authored : {Size{1280, 720}, Size{720, 1280}, Size{1024, 1024}}) {
        for (const double dpi : {1.0, 1.5, 2.75}) {
            const Rect viewport{31 * dpi, 72 * dpi, 900 * dpi, 500 * dpi};
            const Point center{authored.width_ * .5, authored.height_ * .5};
            Near(ScreenToCanvas(CanvasToScreen(center, authored, viewport), authored, viewport)
                         .value(),
                 center);
            const auto fit = AspectFit(authored, viewport);
            Require(!ScreenToCanvas({fit.x_ - 1, fit.y_ - 1}, authored, viewport),
                    "letterbox is not an object hit");
            Require(ScreenToCanvas({fit.x_ - 1, fit.y_ - 1}, authored, viewport, true).has_value(),
                    "captured drag can leave image");
        }
    }
}
}  // namespace
int main() {
    try {
        Run();
        std::cout << "2D affine: signed parent scale, inverse, pivot preservation, aspect/DPI and "
                     "hit bounds passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
