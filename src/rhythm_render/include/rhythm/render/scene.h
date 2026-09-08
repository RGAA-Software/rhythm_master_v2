#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "rhythm/render/texture_handle.h"

namespace rhythm::render {
struct MeshHandle {
    std::uint64_t device_ = 0;
    std::uint32_t slot_ = 0;
    std::uint32_t generation_ = 0;
    bool operator==(const MeshHandle&) const = default;
};
struct MeshVertex {
    float x_ = 0;
    float y_ = 0;
    float z_ = 0;
    float normal_x_ = 0;
    float normal_y_ = 0;
    float normal_z_ = 1;
    float u_ = 0;
    float v_ = 0;
    std::array<float, 4> tangent_{1, 0, 0, 1};
};
using Matrix4 = std::array<float, 16>;
inline constexpr Matrix4 kIdentityMatrix{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
struct MaterialTextures {
    // base color, tangent-space +Y normal, occlusion/roughness/metallic, emission.
    // Color slots unpremultiply their RGB; coverage remains the draw color alpha.
    // Data slots are opaque raw values. All slots use top-left UV and repeat.
    std::array<TextureHandle, 4> slots_{};
    bool color_srgb_ = true;
    float normal_scale_ = 1;
    std::array<float, 4> uv_transform_{1, 1, 0, 0};
    bool operator==(const MaterialTextures&) const = default;
};
struct MeshDraw {
    MeshHandle mesh_{};
    Matrix4 model_ = kIdentityMatrix;
    std::array<float, 4> color_{1, 1, 1, 1};
    bool double_sided_ = false;
    Matrix4 normal_ = kIdentityMatrix;
    float metallic_ = 0;
    float roughness_ = 0.5f;
    std::array<float, 3> emissive_{};
    bool unlit_ = true;
    MaterialTextures textures_{};
};
struct DirectionalLight {
    // Unit vector from the surface toward the light; linear RGB radiance.
    std::array<float, 3> direction_{0, 0, 1};
    std::array<float, 3> radiance_{1, 1, 1};
};
struct PositionalLight {
    std::array<float, 3> position_{0, 0, 3};
    std::array<float, 3> radiance_{1, 1, 1};
    // Spot axis points from the light toward the scene; ignored for point lights.
    std::array<float, 3> direction_{0, 0, -1};
    float range_ = 10;
    float decay_ = 2;
    bool spot_ = false;
    float cone_angle_ = 45;  // Half angle, degrees.
    float cone_decay_ = 1;
};
// Canonical right-handed matrices with clip Z in [-1,+1]. Adapters map to their
// depth range and target orientation. Draw records hold stable resource handles.
struct SceneDrawList {
    Matrix4 view_ = kIdentityMatrix;
    Matrix4 projection_ = kIdentityMatrix;
    std::vector<MeshDraw> draws_{};
    std::array<float, 3> camera_position_{0, 0, 3};
    std::vector<DirectionalLight> lights_{};
    std::array<float, 3> camera_backward_{0, 0, 1};
    bool orthographic_ = false;
    // Shared budget: at most four directional + positional lights per pass.
    std::vector<PositionalLight> positional_lights_{};
};
}  // namespace rhythm::render
