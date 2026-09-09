#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>

#include "rhythm/geometry2d/vector.h"

namespace {
using namespace rhythm::geometry2d;
void Check(bool value) {
    if (!value) throw std::runtime_error("vector.contract");
}
double Area(const VectorMesh& mesh) {
    Check(mesh.indices_.size() % 3 == 0);
    double result = 0;
    for (std::size_t i = 0; i < mesh.indices_.size(); i += 3) {
        const auto a = mesh.vertices_.at(mesh.indices_[i]);
        const auto b = mesh.vertices_.at(mesh.indices_[i + 1]);
        const auto c = mesh.vertices_.at(mesh.indices_[i + 2]);
        result += std::abs((b.x_ - a.x_) * (c.y_ - a.y_) - (b.y_ - a.y_) * (c.x_ - a.x_)) / 2;
    }
    return result;
}
template <class Function>
void Reject(Function function) {
    bool rejected = false;
    try {
        function();
    } catch (const std::exception&) {
        rejected = true;
    }
    Check(rejected);
}
}  // namespace
int main() {
    try {
        const Contour square{{{0, 0}, {10, 0}, {10, 10}, {0, 10}}, true};
        const Contour hole{{{3, 3}, {7, 3}, {7, 7}, {3, 7}}, true};
        const Contour island{{{4, 4}, {6, 4}, {6, 6}, {4, 6}}, true};
        Check(Area(FillContours(std::array{square})) == 100);
        Check(Area(FillContours(std::array{square, hole})) == 84);
        Check(Area(FillContours(std::array{square, hole, island})) == 88);
        const Contour concave{{{0, 0}, {4, 0}, {4, 1}, {1, 1}, {1, 4}, {0, 4}}, true};
        Check(Area(FillContours(std::array{concave})) == 7);
        const Contour bowtie{{{0, 0}, {2, 2}, {0, 2}, {2, 0}}, true};
        Check(Area(FillContours(std::array{bowtie})) == 2);
        Check(FillContours({}).indices_.empty());
        const auto mesh = FillContours(std::array{square, hole, island});
        Check(mesh == FillContours(std::array{square, hole, island}));
        const Contour line{{{0, 0}, {10, 0}}, false};
        StrokeStyle style;
        style.cap_ = LineCap::kButt;
        Check(std::abs(Area(StrokeContours(std::array{line}, style)) - 20) < .001);
        style.cap_ = LineCap::kSquare;
        Check(std::abs(Area(StrokeContours(std::array{line}, style)) - 24) < .001);
        style.cap_ = LineCap::kRound;
        style.arc_tolerance_ = .001;
        Check(std::abs(Area(StrokeContours(std::array{line}, style)) - 20 - std::numbers::pi) <
              .03);
        style.join_ = LineJoin::kMiter;
        Check(std::abs(Area(StrokeContours(std::array{square}, style)) - 80) < .001);
        style.width_ = 0;
        Check(StrokeContours(std::array{line}, style).indices_.empty());
        Reject([&] { FillContours(std::array{line}); });
        auto invalid = square;
        invalid.points_[0].x_ = std::numeric_limits<double>::quiet_NaN();
        Reject([&] { FillContours(std::array{invalid}); });
        invalid = square;
        invalid.points_.resize(kMaximumVectorPoints + 1);
        Reject([&] { FillContours(std::array{invalid}); });
        style.width_ = -1;
        Reject([&] { StrokeContours(std::array{line}, style); });
        std::cout << "Vector concavity, even-odd holes/islands/intersections, caps, joins and "
                     "budgets passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
