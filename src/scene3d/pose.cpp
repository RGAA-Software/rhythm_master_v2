#include "rhythm/scene/pose.h"

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <stdexcept>

namespace rhythm::scene {
Matrix ComposeEuler(const EulerPose& pose) {
    const auto native =
            glm::translate(glm::dmat4(1), glm::dvec3(pose.translation_.x_, pose.translation_.y_,
                                                     pose.translation_.z_)) *
            glm::eulerAngleZYX(glm::radians(pose.degrees_.z_), glm::radians(pose.degrees_.y_),
                               glm::radians(pose.degrees_.x_)) *
            glm::scale(glm::dmat4(1), glm::dvec3(pose.scale_.x_, pose.scale_.y_, pose.scale_.z_));
    Matrix result;
    for (int column = 0; column < 4; ++column)
        for (int row = 0; row < 4; ++row)
            result.values_[std::size_t(column * 4 + row)] = native[column][row];
    if (!ValidAffine(result)) throw std::invalid_argument("scene.euler_pose");
    return result;
}
std::optional<EulerPose> DecomposeEuler(const Matrix& matrix) {
    if (!ValidAffine(matrix)) return {};
    const auto native = glm::make_mat4(matrix.values_.data());
    glm::dmat3 rotation(native);
    glm::dvec3 scale{glm::length(rotation[0]), glm::length(rotation[1]), glm::length(rotation[2])};
    for (int column = 0; column < 3; ++column) {
        if (!std::isfinite(scale[column]) || scale[column] < 1e-12) return {};
        rotation[column] /= scale[column];
    }
    if (std::abs(glm::dot(rotation[0], rotation[1])) > 1e-6 ||
        std::abs(glm::dot(rotation[0], rotation[2])) > 1e-6 ||
        std::abs(glm::dot(rotation[1], rotation[2])) > 1e-6)
        return {};
    if (glm::determinant(rotation) < 0) {
        rotation[0] *= -1;
        scale[0] *= -1;
    }
    double z = 0, y = 0, x = 0;
    glm::extractEulerAngleZYX(glm::dmat4(rotation), z, y, x);
    EulerPose result{{matrix.values_[12], matrix.values_[13], matrix.values_[14]},
                     {glm::degrees(x), glm::degrees(y), glm::degrees(z)},
                     {scale.x, scale.y, scale.z}};
    const auto recomposed = ComposeEuler(result);
    for (std::size_t index = 0; index < matrix.values_.size(); ++index)
        if (std::abs(recomposed.values_[index] - matrix.values_[index]) >
            2e-6 * std::max(1.0, std::abs(matrix.values_[index])))
            return {};
    return result;
}
}  // namespace rhythm::scene
