#pragma once

#include <array>
#include <cstdint>
#include <vector>

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
};
using Matrix4 = std::array<float, 16>;
inline constexpr Matrix4 kIdentityMatrix{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
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
};
struct DirectionalLight {
    // Unit vector from the surface toward the light; linear RGB radiance.
    std::array<float, 3> direction_{0, 0, 1};
    std::array<float, 3> radiance_{1, 1, 1};
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
};
}  // namespace rhythm::render
