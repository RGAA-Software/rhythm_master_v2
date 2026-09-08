#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "rhythm/scene/animation.h"
#include "rhythm/scene/math.h"

namespace rhythm::scene {
using NodeId = std::uint64_t;
struct Color {
    float red_ = 1;
    float green_ = 1;
    float blue_ = 1;
    float alpha_ = 1;
};
struct MaterialTextures {
    // Texture producer IDs belong to the immutable publishing graph generation.
    // Scene3D stores identities only; Runtime resolves them for each frame.
    std::array<std::uint64_t, 4> nodes_{};
    // Optional indices into the owning model image array; graph nodes override a slot.
    std::array<std::optional<std::uint32_t>, 4> images_{};
    bool color_srgb_ = true;
    float normal_scale_ = 1;
    std::array<float, 4> uv_transform_{1, 1, 0, 0};
};
struct Material {
    Color base_color_{};
    Vector3 emissive_{};
    float metallic_ = 0;
    float roughness_ = 0.5f;
    bool unlit_ = true;
    bool double_sided_ = false;
    MaterialTextures textures_{};
};
struct Vertex {
    float x_ = 0;
    float y_ = 0;
    float z_ = 0;
    float normal_x_ = 0;
    float normal_y_ = 0;
    float normal_z_ = 1;
    float u_ = 0;
    float v_ = 0;
    // MikkTSpace tangent.xyz and handedness. Used only when mesh has_tangents_.
    std::array<float, 4> tangent_{1, 0, 0, 1};
};
struct Mesh {
    std::vector<Vertex> vertices_{};
    std::vector<std::uint32_t> indices_{};
    std::uint32_t material_ = 0;
    bool has_tangents_ = false;
    struct JointWeights {
        std::array<std::uint8_t, 4> joints_{};
        std::array<float, 4> weights_{1, 0, 0, 0};
    };
    std::vector<JointWeights> skin_{};
    struct MorphVertex {
        std::array<float, 3> position_{};
        std::array<float, 3> normal_{};
        std::array<float, 3> tangent_{};
    };
    struct MorphTarget {
        std::vector<MorphVertex> deltas_{};
    };
    std::vector<MorphTarget> morphs_{};
};
struct Node {
    NodeId id_ = 0;
    std::optional<NodeId> parent_{};
    Matrix local_{};
    std::vector<std::uint32_t> meshes_{};
    bool visible_ = true;
    std::string name_{};
    std::optional<std::uint32_t> skin_{};
};
// Immutable after publication. Mesh/material indices address these owned arrays;
// hierarchy uses stable value IDs. GPU and parser resources are not retained.
struct TextureImage {
    std::uint16_t width_ = 0;
    std::uint16_t height_ = 0;
    // Owned straight RGBA8, top-left row first. Opaque GLB profile forces alpha 255.
    std::vector<std::uint8_t> rgba_{};
};
inline constexpr std::size_t kMaximumModelImageBytes = 64 * 1024 * 1024;
struct Skin {
    std::vector<NodeId> joints_{};
    std::vector<Matrix> inverse_bind_{};
};
inline constexpr std::size_t kMaximumModelSkinBones = 48;
struct Model {
    std::vector<Mesh> meshes_{};
    std::vector<Material> materials_{};
    std::vector<Node> nodes_{};
    std::vector<TextureImage> images_{};
    AnimationPose rest_pose_{};
    std::vector<AnimationClip> animations_{};
    std::vector<Skin> skins_{};
};
struct WorldNode {
    Matrix transform_{};
    bool visible_ = true;
};
void Validate(const Model& model);
std::map<NodeId, WorldNode> WorldTransforms(const Model& model, const AnimationPose& pose = {});
void ValidateAnimations(const Model& model);
void ValidateSkins(const Model& model);
void ValidateMorphs(const Model& model);
// Skin palettes are relative to each bound mesh node; model/instance transforms
// are applied separately at drawing. Non-joint ancestors remain in the hierarchy.
std::map<NodeId, std::vector<Matrix>> SkinPalettes(const Model& model,
                                                   const std::map<NodeId, WorldNode>& worlds);
void GenerateNormals(Mesh& mesh);
// Synchronous, bounded MikkTSpace generation. Splits incompatible corner frames;
// publishes the replacement only on success. Input must have unit normals/UVs.
void GenerateTangents(Mesh& mesh);
Model Cube();
// Godot-derived UV sphere/ellipsoid. Radius and height are positive scene units;
// radial segments 3..256, intermediate rings 1..128. Triangle winding is CCW.
Model Sphere(float radius = 0.5f, float height = 1, std::uint32_t radial_segments = 32,
             std::uint32_t rings = 16);
// Major radius and tube/major radius ratio. Both seams close exactly; CCW faces.
Model Torus(float radius = 0.8f, float tube_ratio = 0.08f, std::uint32_t segments = 64,
            std::uint32_t tube_segments = 16);
}  // namespace rhythm::scene
