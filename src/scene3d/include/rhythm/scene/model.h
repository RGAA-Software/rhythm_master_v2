#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

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
};
struct Node {
    NodeId id_ = 0;
    std::optional<NodeId> parent_{};
    Matrix local_{};
    std::vector<std::uint32_t> meshes_{};
    bool visible_ = true;
    std::string name_{};
};
// Immutable after publication. Mesh/material indices address these owned arrays;
// hierarchy uses stable value IDs. GPU and parser resources are not retained.
struct Model {
    std::vector<Mesh> meshes_{};
    std::vector<Material> materials_{};
    std::vector<Node> nodes_{};
};
struct WorldNode {
    Matrix transform_{};
    bool visible_ = true;
};
void Validate(const Model& model);
std::map<NodeId, WorldNode> WorldTransforms(const Model& model);
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
