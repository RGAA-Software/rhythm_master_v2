#pragma once

#include <memory>

#include "rhythm/scene/model.h"

namespace rhythm::scene {
// Stable identity belongs to the publishing graph/runtime generation. Geometry is
// immutable and shared by instances; transforms never duplicate mesh arrays.
struct Geometry {
    std::uint64_t id_ = 0;
    std::uint64_t revision_ = 0;
    std::shared_ptr<const Model> model_{};
};
struct Instance {
    std::shared_ptr<const Geometry> geometry_{};
    Matrix transform_{};
    std::optional<Material> material_{};
};
struct Scene {
    std::vector<Instance> instances_{};
    struct DirectionalLight {
        Vector3 direction_{0, 0, 1};
        Vector3 radiance_{1, 1, 1};
    };
    std::vector<DirectionalLight> lights_{};
};
}  // namespace rhythm::scene
