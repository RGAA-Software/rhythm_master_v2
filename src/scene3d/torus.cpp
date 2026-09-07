// Adapted from Godot 4.5.1 TorusMesh::_create_mesh_array, primitive_meshes.cpp.
// Copyright (c) 2014-present Godot Engine contributors (retained AUTHORS.md).
// Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.
// MIT license: third_party/notices/godot/LICENSE.txt; provenance/godot_3d.json.
#include <cmath>
#include <numbers>
#include <stdexcept>

#include "rhythm/scene/model.h"

namespace rhythm::scene {
Model Torus(float radius, float tube_ratio, std::uint32_t segments, std::uint32_t tube_segments) {
    if (!std::isfinite(radius) || radius < 0.001f || radius > 100 || !std::isfinite(tube_ratio) ||
        tube_ratio < 0.01f || tube_ratio > 0.9f || segments < 3 || segments > 256 ||
        tube_segments < 3 || tube_segments > 128)
        throw std::invalid_argument("scene.torus");
    Mesh mesh;
    mesh.vertices_.reserve((segments + 1) * (tube_segments + 1));
    mesh.indices_.reserve(segments * tube_segments * 6);
    const auto tube = radius * tube_ratio;
    for (std::uint32_t i = 0; i <= segments; ++i) {
        const auto u = static_cast<float>(i) / static_cast<float>(segments);
        const auto angle = u * 2 * std::numbers::pi_v<float>;
        const auto nx = i == segments ? 0.0f : -std::sin(angle);
        const auto nz = i == segments ? -1.0f : -std::cos(angle);
        for (std::uint32_t j = 0; j <= tube_segments; ++j) {
            const auto v = static_cast<float>(j) / static_cast<float>(tube_segments);
            const auto tube_angle = v * 2 * std::numbers::pi_v<float>;
            const auto tx = j == tube_segments ? -1.0f : -std::cos(tube_angle);
            const auto ty = j == tube_segments ? 0.0f : std::sin(tube_angle);
            mesh.vertices_.push_back({nx * (radius + tx * tube), ty * tube,
                                      nz * (radius + tx * tube), nx * tx, ty, nz * tx, u, v});
            if (i > 0 && j > 0) {
                const auto current = i * (tube_segments + 1);
                const auto previous = current - tube_segments - 1;
                for (const auto index : {current + j - 1, previous + j - 1, previous + j,
                                         current + j - 1, previous + j, current + j})
                    mesh.indices_.push_back(index);
            }
        }
    }
    Model model;
    model.materials_.emplace_back();
    model.meshes_.push_back(std::move(mesh));
    model.nodes_.push_back({1, {}, {}, {0}, true, "Torus"});
    return model;
}
}  // namespace rhythm::scene
