#pragma once

#include <array>

namespace rhythm::scene {
struct Vector3 {
    double x_ = 0;
    double y_ = 0;
    double z_ = 0;
    bool operator==(const Vector3&) const = default;
};
struct Quaternion {
    double x_ = 0;
    double y_ = 0;
    double z_ = 0;
    double w_ = 1;
};
// Column-major, column vectors, right handed, +Y up and camera forward -Z.
struct Matrix {
    std::array<double, 16> values_{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    bool operator==(const Matrix&) const = default;
};
double Dot(Vector3 first, Vector3 second);
Vector3 Cross(Vector3 first, Vector3 second);
Vector3 Normalize(Vector3 vector);
Matrix Multiply(const Matrix& first, const Matrix& second);
Matrix Compose(Vector3 translation, Quaternion rotation, Vector3 scale);
Vector3 TransformPoint(const Matrix& transform, Vector3 point);
Vector3 TransformNormal(const Matrix& transform, Vector3 normal);
// Unnormalized inverse-transpose basis for GPU interpolation, then normalization.
Matrix NormalTransform(const Matrix& transform);
bool ValidAffine(const Matrix& matrix);
}  // namespace rhythm::scene
