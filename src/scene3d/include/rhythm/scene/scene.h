#pragma once

#include <memory>

#include "rhythm/scene/model.h"

namespace rhythm::scene {
struct Deformation {
    double twist_ = 0;  // Degrees per local unit; right-handed rotation.
    double taper_ = 0;
    std::uint32_t axis_ = 1;
    Vector3 pivot_{};
};
// Stable identity belongs to the publishing graph/runtime generation. Geometry is
// immutable and shared by instances; transforms never duplicate mesh arrays.
struct Geometry {
    std::uint64_t id_ = 0;
    std::uint64_t revision_ = 0;
    std::shared_ptr<const Model> model_{};
    // Modifiers share the immutable base model and its upload identity. Zero
    // selects id_/revision_ for an ordinary, unmodified source geometry.
    std::uint64_t upload_id_ = 0;
    std::uint64_t upload_revision_ = 0;
    std::vector<Deformation> deformations_{};
    std::shared_ptr<const AnimationPose> pose_{};
};
struct Instance {
    std::shared_ptr<const Geometry> geometry_{};
    Matrix transform_{};
    std::optional<Material> material_{};
};
struct ShadowSettings {
    std::uint32_t light_ = 0;
    std::uint16_t resolution_ = 1024;
    Vector3 center_{};
    double extent_ = 10;
    double distance_ = 20;
    double near_ = 0.05;
    float depth_bias_ = 0.001f;
    float normal_bias_ = 0.01f;
    bool filter_ = true;
};
// World environment stays fixed when scene geometry is transformed.
struct EnvironmentSettings {
    std::uint64_t texture_node_ = 0;
    float energy_ = 1;
    float rotation_ = 0;
    bool source_srgb_ = true;
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
    std::optional<ShadowSettings> shadow_{};
    std::optional<EnvironmentSettings> environment_{};
};
}  // namespace rhythm::scene
