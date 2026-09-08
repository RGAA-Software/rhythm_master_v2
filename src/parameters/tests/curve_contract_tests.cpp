#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "rhythm/parameters/curve.h"

namespace {
void Check(bool value) {
    if (!value) throw std::runtime_error("curve.contract");
}
}  // namespace
int main() {
    using namespace rhythm::parameters;
    try {
        Curve curve({{0, 1, Interpolation::kStep},
                     {1, 3, Interpolation::kLinear},
                     {3, 7, Interpolation::kSmooth},
                     {5, -1}});
        Check(curve.Evaluate(-10) == 1 && curve.Evaluate(0.999) == 1);
        Check(curve.Evaluate(1) == 3 && curve.Evaluate(2) == 5);
        Check(curve.Evaluate(3.5) == 5.75 && curve.Evaluate(4) == 3);
        Check(curve.Evaluate(10) == -1);
        const auto original = curve;
        for (const auto& invalid :
             std::vector<std::vector<Keyframe>>{{},
                                                {{0, 0}, {0, 1}},
                                                {{2, 0}, {1, 1}},
                                                {{-1, 0}},
                                                {{0, std::numeric_limits<double>::infinity()}},
                                                {{0, 0, static_cast<Interpolation>(99)}}}) {
            bool rejected = false;
            try {
                curve.SetKeys(invalid);
            } catch (const std::exception&) {
                rejected = true;
            }
            Check(rejected && curve == original);
        }
        curve.SetKeys({{4, 8}});
        Check(curve.Evaluate(0) == 8 && curve.Evaluate(1.0e300) == 8);
        curve = Curve{};
        Check(curve.Evaluate(0.5) == 0.5);
        curve = Curve(
                {{0, 0, Interpolation::kHermite, 0, 1}, {2, 1, Interpolation::kLinear, 0, 0}});
        Check(curve.Evaluate(0.5) == 0.4375 && curve.Evaluate(1) == 0.75);
        Check(curve.Evaluate(-1) == 0 && curve.Evaluate(3) == 1);
        const std::array<std::size_t, 2> all{0, 1};
        const CurveTransform transform{3, 2, 0, -1, 4, 0};
        const auto scaled = TransformKeys(curve, all, transform);
        Check(scaled.Keys()[0].out_slope_ == 2 && scaled.Keys()[1].seconds_ == 7);
        for (int i = 0; i <= 20; ++i) {
            const auto time = double(i) / 10;
            Check(std::abs(scaled.Evaluate(time * 2 + 3) - (curve.Evaluate(time) * 4 - 1)) < 1e-10);
        }
        const std::array<std::size_t, 1> first{0};
        for (const CurveTransform invalid :
             {CurveTransform{2}, CurveTransform{0, 0}, CurveTransform{-1}, CurveTransform{0, -1}}) {
            bool rejected = false;
            try {
                (void)TransformKeys(curve, first, invalid);
            } catch (const std::exception&) {
                rejected = true;
            }
            Check(rejected && curve.Evaluate(0.5) == 0.4375);
        }
        bool rejected = false;
        try {
            (void)Curve(
                    {{0, 0, Interpolation::kHermite, 0, std::numeric_limits<double>::infinity()}});
        } catch (const std::exception&) {
            rejected = true;
        }
        Check(rejected);
        std::cout << "curve contracts passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
