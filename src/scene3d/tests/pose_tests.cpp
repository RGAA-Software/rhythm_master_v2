#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "rhythm/scene/pose.h"

namespace {
void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void Run() {
    using namespace rhythm::scene;
    for (int index = 0; index < 100; ++index) {
        const auto angle = index * 19.3;
        EulerPose pose{{.1 * index, 2, -3},
                       {angle, 90 + 3.7 * index, -angle},
                       {index % 2 ? -2.0 : 2.0, .7, 1.3}};
        const auto matrix = ComposeEuler(pose);
        const auto extracted = DecomposeEuler(matrix);
        Check(extracted.has_value(), "valid pose round trip including gimbal lock and mirror");
        const auto after = ComposeEuler(*extracted);
        for (std::size_t element = 0; element < matrix.values_.size(); ++element)
            Check(std::abs(matrix.values_[element] - after.values_[element]) < 1e-8,
                  "Euler extraction preserves matrix");
    }
    const auto x = 17.0 * 3.141592653589793 / 360;
    const auto y = 31.0 * 3.141592653589793 / 360;
    const auto z = -46.0 * 3.141592653589793 / 360;
    const auto original =
            Multiply(Compose({1, 2, 3}, {0, 0, std::sin(z), std::cos(z)}, {1, 1, 1}),
                     Multiply(Compose({}, {0, std::sin(y), 0, std::cos(y)}, {1, 1, 1}),
                              Compose({}, {std::sin(x), 0, 0, std::cos(x)}, {2, .5, 3})));
    const auto shared = ComposeEuler({{1, 2, 3}, {17, 31, -46}, {2, .5, 3}});
    for (std::size_t element = 0; element < original.values_.size(); ++element)
        Check(std::abs(original.values_[element] - shared.values_[element]) < 1e-10,
              "same intrinsic order as existing graph transform");
    Matrix shear;
    shear.values_[4] = .2;
    Check(!DecomposeEuler(shear), "shear is not silently turned into TRS");
    shear.values_[4] = 0;
    shear.values_[0] = 0;
    Check(!DecomposeEuler(shear), "singular rejected");
    shear.values_[0] = std::numeric_limits<double>::infinity();
    Check(!DecomposeEuler(shear), "nonfinite rejected");
}
}  // namespace
int main() {
    try {
        Run();
        std::cout << "Shared Euler poses: render order, mirrored/gimbal round trips, "
                     "shear/singular guards passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
