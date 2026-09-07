// Projection equations adapted from Godot 4.5.1 core/math/projection.cpp.
// Copyright (c) 2014-present Godot Engine contributors (see retained AUTHORS.md).
// Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.
// MIT license: third_party/notices/godot/LICENSE.txt. See provenance/godot_3d.json.
#include "rhythm/scene/camera.h"

#include <cmath>
#include <numbers>
#include <stdexcept>

namespace rhythm::scene {
Matrix View(const Camera& camera) {
    const auto backward =
            Normalize({camera.eye_.x_ - camera.target_.x_, camera.eye_.y_ - camera.target_.y_,
                       camera.eye_.z_ - camera.target_.z_});
    const auto right = Normalize(Cross(camera.up_, backward));
    const auto up = Cross(backward, right);
    Matrix result;
    result.values_ = {right.x_,
                      up.x_,
                      backward.x_,
                      0,
                      right.y_,
                      up.y_,
                      backward.y_,
                      0,
                      right.z_,
                      up.z_,
                      backward.z_,
                      0,
                      -Dot(right, camera.eye_),
                      -Dot(up, camera.eye_),
                      -Dot(backward, camera.eye_),
                      1};
    if (!ValidAffine(result)) throw std::invalid_argument("scene.camera");
    return result;
}
Matrix Projection(const Camera& camera, double aspect) {
    if (!std::isfinite(aspect) || aspect < 1.0 / 256 || aspect > 256 ||
        !std::isfinite(camera.near_) || !std::isfinite(camera.far_) || camera.near_ < 0.001 ||
        camera.far_ <= camera.near_ || camera.far_ > 1e6)
        throw std::invalid_argument("scene.projection");
    Matrix result;
    result.values_.fill(0);
    const auto depth = camera.far_ - camera.near_;
    if (camera.kind_ == ProjectionKind::kPerspective) {
        if (!std::isfinite(camera.vertical_fov_) || camera.vertical_fov_ < 1 ||
            camera.vertical_fov_ > 179)
            throw std::invalid_argument("scene.field_of_view");
        const auto cotangent = 1 / std::tan(camera.vertical_fov_ * std::numbers::pi / 360);
        result.values_[0] = cotangent / aspect;
        result.values_[5] = cotangent;
        result.values_[10] = -(camera.far_ + camera.near_) / depth;
        result.values_[11] = -1;
        result.values_[14] = -2 * camera.near_ * camera.far_ / depth;
    } else if (camera.kind_ == ProjectionKind::kOrthographic) {
        if (!std::isfinite(camera.orthographic_height_) || camera.orthographic_height_ < 0.001 ||
            camera.orthographic_height_ > 1e6)
            throw std::invalid_argument("scene.orthographic_size");
        result.values_[0] = 2 / (camera.orthographic_height_ * aspect);
        result.values_[5] = 2 / camera.orthographic_height_;
        result.values_[10] = -2 / depth;
        result.values_[14] = -(camera.far_ + camera.near_) / depth;
        result.values_[15] = 1;
    } else
        throw std::invalid_argument("scene.projection_kind");
    return result;
}
}  // namespace rhythm::scene
