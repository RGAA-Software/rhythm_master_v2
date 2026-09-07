// Adapted from Godot 4.5.1 SphereMesh::create_mesh_array in primitive_meshes.cpp.
// Copyright (c) 2014-present Godot Engine contributors (see retained AUTHORS.md).
// Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.
// MIT license: third_party/notices/godot/LICENSE.txt. See provenance/godot_3d.json.
#include <cmath>
#include <numbers>
#include <stdexcept>

#include "rhythm/scene/model.h"

namespace rhythm::scene {
Model Sphere(float radius, float height, std::uint32_t radial_segments, std::uint32_t rings) {
    if (!std::isfinite(radius) || !std::isfinite(height) || radius < 0.001f || radius > 10000 ||
        height < 0.001f || height > 10000 || radial_segments < 3 || radial_segments > 256 ||
        rings < 1 || rings > 128) {
        throw std::invalid_argument("scene.sphere");
    }
    Mesh mesh;
    mesh.vertices_.reserve((rings + 2) * (radial_segments + 1));
    mesh.indices_.reserve((rings + 1) * radial_segments * 6);
    const auto scale = height / radius * 0.5f;
    std::uint32_t this_row = 0, previous_row = 0;
    for (std::uint32_t j = 0; j <= rings + 1; ++j) {
        const auto v = static_cast<float>(j) / static_cast<float>(rings + 1);
        const auto w = j == rings + 1 ? 0.0f : std::sin(std::numbers::pi_v<float> * v);
        const auto y = j == rings + 1 ? -1.0f : std::cos(std::numbers::pi_v<float> * v);
        for (std::uint32_t i = 0; i <= radial_segments; ++i) {
            const auto u = static_cast<float>(i) / static_cast<float>(radial_segments);
            const auto x =
                    i == radial_segments ? 0.0f : std::sin(u * 2 * std::numbers::pi_v<float>);
            const auto z =
                    i == radial_segments ? 1.0f : std::cos(u * 2 * std::numbers::pi_v<float>);
            const auto normal = Normalize({x * w * scale, y, z * w * scale});
            mesh.vertices_.push_back({x * radius * w, y * scale * radius, z * radius * w,
                                      static_cast<float>(normal.x_), static_cast<float>(normal.y_),
                                      static_cast<float>(normal.z_), u, v});
            if (i > 0 && j > 0) {
                // Godot uses clockwise front faces; our mesh contract uses CCW.
                for (const auto index : {previous_row + i - 1, this_row + i - 1, previous_row + i,
                                         previous_row + i, this_row + i - 1, this_row + i}) {
                    mesh.indices_.push_back(index);
                }
            }
        }
        previous_row = this_row;
        this_row = static_cast<std::uint32_t>(mesh.vertices_.size());
    }
    Model model;
    model.materials_.emplace_back();
    model.meshes_.push_back(std::move(mesh));
    model.nodes_.push_back({1, {}, {}, {0}, true, "Sphere"});
    return model;
}
}  // namespace rhythm::scene
