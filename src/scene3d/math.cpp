#include "rhythm/scene/math.h"

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stdexcept>

namespace rhythm::scene {
namespace {
glm::dvec3 Native(Vector3 value) { return {value.x_, value.y_, value.z_}; }
Vector3 Value(glm::dvec3 value) { return {value.x, value.y, value.z}; }
Matrix Value(const glm::dmat4& native) {
    Matrix result;
    for (int column = 0; column < 4; ++column)
        for (int row = 0; row < 4; ++row)
            result.values_[static_cast<std::size_t>(column * 4 + row)] = native[column][row];
    return result;
}
}  // namespace
double Dot(Vector3 a, Vector3 b) { return glm::dot(Native(a), Native(b)); }
Vector3 Cross(Vector3 a, Vector3 b) { return Value(glm::cross(Native(a), Native(b))); }
Vector3 Normalize(Vector3 value) {
    const auto length = glm::length(Native(value));
    if (!std::isfinite(length) || length < 1e-12) throw std::invalid_argument("scene.direction");
    return Value(Native(value) / length);
}
Matrix Multiply(const Matrix& a, const Matrix& b) {
    return Value(glm::make_mat4(a.values_.data()) * glm::make_mat4(b.values_.data()));
}
Matrix InverseAffine(const Matrix& matrix) {
    if (!ValidAffine(matrix)) throw std::invalid_argument("scene.transform");
    const auto native = glm::make_mat4(matrix.values_.data());
    const auto determinant = glm::determinant(glm::dmat3(native));
    if (!std::isfinite(determinant) || std::abs(determinant) < 1e-12)
        throw std::invalid_argument("scene.singular_transform");
    auto result = Value(glm::inverse(native));
    // Affine structure is exact in the public contract; discard roundoff only
    // in the constant homogeneous row, not in the transformation basis.
    result.values_[3] = result.values_[7] = result.values_[11] = 0;
    result.values_[15] = 1;
    if (!ValidAffine(result)) throw std::invalid_argument("scene.transform");
    return result;
}
Matrix Compose(Vector3 translation, Quaternion rotation, Vector3 scale) {
    const glm::dquat quaternion(rotation.w_, rotation.x_, rotation.y_, rotation.z_);
    const auto length = glm::length(quaternion);
    if (!std::isfinite(length) || length < 1e-12) throw std::invalid_argument("scene.rotation");
    const auto matrix =
            Value(glm::translate(glm::dmat4(1), Native(translation)) *
                  glm::mat4_cast(quaternion / length) * glm::scale(glm::dmat4(1), Native(scale)));
    if (!ValidAffine(matrix)) throw std::invalid_argument("scene.transform");
    return matrix;
}
Vector3 TransformPoint(const Matrix& matrix, Vector3 point) {
    const auto transformed = glm::make_mat4(matrix.values_.data()) * glm::dvec4(Native(point), 1);
    if (!std::isfinite(transformed.w) || std::abs(transformed.w) < 1e-12)
        throw std::invalid_argument("scene.point_at_infinity");
    return Value(glm::dvec3(transformed) / transformed.w);
}
Vector3 TransformNormal(const Matrix& matrix, Vector3 normal) {
    return Normalize(TransformPoint(NormalTransform(matrix), normal));
}
Matrix NormalTransform(const Matrix& matrix) {
    const glm::dmat3 basis(glm::make_mat4(matrix.values_.data()));
    const auto determinant = glm::determinant(basis);
    if (!std::isfinite(determinant) || std::abs(determinant) < 1e-12)
        throw std::invalid_argument("scene.singular_transform");
    const auto result = Value(glm::dmat4(glm::transpose(glm::inverse(basis))));
    if (!ValidAffine(result)) throw std::invalid_argument("scene.normal_transform");
    return result;
}
bool ValidAffine(const Matrix& matrix) {
    const auto& m = matrix.values_;
    return std::all_of(m.begin(), m.end(),
                       [](double v) { return std::isfinite(v) && std::abs(v) <= 1e6; }) &&
           m[3] == 0 && m[7] == 0 && m[11] == 0 && m[15] == 1;
}
}  // namespace rhythm::scene
