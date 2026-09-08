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
    struct PositionalLight {
        Vector3 position_{0, 0, 3};
        Vector3 radiance_{1, 1, 1};
        Vector3 direction_{0, 0, -1};
        double range_ = 10;
        double decay_ = 2;
        bool spot_ = false;
        double cone_angle_ = 45;
        double cone_decay_ = 1;
    };
    // All light kinds share the four-light budget. Range is in world units;
    // scene transforms change positions/axes but do not rescale that range.
    std::vector<PositionalLight> positional_lights_{};
};
}  // namespace rhythm::scene
