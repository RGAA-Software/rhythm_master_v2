#include <algorithm>
#include <array>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <iostream>
#include <numbers>
#include <stdexcept>

#include "gpu_execution_probe.h"
#include "rhythm/render/renderer.h"

namespace rhythm::validation {
namespace {
glm::dvec3 Position(glm::dvec3 position, std::span<const render::MeshDeformation> modifiers) {
    for (const auto& modifier : modifiers) {
        const glm::dvec3 pivot(modifier.pivot_[0], modifier.pivot_[1], modifier.pivot_[2]);
        glm::dvec3 axis(0);
        axis[int(modifier.axis_)] = 1;
        auto local = position - pivot;
        const auto along = glm::dot(local, axis);
        const auto scale = std::clamp(1.0 - modifier.taper_ * along, 0.05, 20.0);
        local = axis * along + (local - axis * along) * scale;
        position =
                pivot + glm::rotate(local, along * modifier.twist_ * std::numbers::pi / 180, axis);
    }
    return position;
}
render::ReadbackImage Complete(render::Renderer& renderer, render::Readback ticket) {
    for (int i = 0; i < 32; ++i) {
        if (auto image = ticket.Poll()) return std::move(*image);
        renderer.BeginFrame();
        renderer.EndFrame();
    }
    throw std::runtime_error("deformation.readback_timeout");
}
}  // namespace
void VerifyMeshDeformation(render::Renderer& renderer) {
    using namespace render;
    std::vector<MeshVertex> original;
    std::vector<std::uint32_t> indices;
    const glm::dvec3 u(1, 0, 0), v(0, std::cos(0.35), std::sin(0.35));
    const auto normal = glm::cross(u, v);
    constexpr std::uint32_t kSegments = 24;
    for (std::uint32_t y = 0; y <= kSegments; ++y)
        for (std::uint32_t x = 0; x <= kSegments; ++x) {
            const auto p = u * (double(x) / kSegments - 0.5) * 1.5 +
                           v * (double(y) / kSegments - 0.5) * 1.5;
            original.push_back({float(p.x), float(p.y), float(p.z), float(normal.x),
                                float(normal.y), float(normal.z), float(x) / kSegments,
                                float(y) / kSegments});
            if (x < kSegments && y < kSegments) {
                const auto a = y * (kSegments + 1) + x, b = a + kSegments + 1;
                indices.insert(indices.end(), {a, a + 1, b, b, a + 1, b + 1});
            }
        }
    auto mesh = renderer.CreateMesh(original, indices);
    auto alternate = renderer.CreateMesh(original, indices);
    auto target = renderer.CreateTexture({128, 128});
    const std::array<std::uint8_t, 4> tilted_normal{180, 156, 238, 255};
    auto texture = renderer.CreateTexture({1, 1}, tilted_normal);
    for (std::uint32_t scenario = 0; scenario < 6; ++scenario) {
        std::vector<MeshDeformation> modifiers{{47, 0.4f, scenario % 3, {0.1f, -0.2f, 0.05f}}};
        if (scenario == 3) modifiers = {{0, 0}};
        if (scenario == 4) modifiers = {{-32, 4, 1}, {35, -0.2f, 2}};
        if (scenario == 5)
            modifiers = {{26, 0.1f, 0}, {-37, -0.2f, 1}, {17, 0.2f, 2}, {23, 0.1f, 1}};
        auto expected = original;
        for (auto& vertex : expected) {
            const glm::dvec3 p(vertex.x_, vertex.y_, vertex.z_);
            const auto transformed = Position(p, modifiers);
            // Independent finite differences verify the shader's analytic normal/TBN.
            const auto du =
                    Position(p + u * 0.00001, modifiers) - Position(p - u * 0.00001, modifiers);
            const auto dv =
                    Position(p + v * 0.00001, modifiers) - Position(p - v * 0.00001, modifiers);
            const auto n = glm::normalize(glm::cross(du, dv));
            const auto t = glm::normalize(du);
            vertex.x_ = float(transformed.x);
            vertex.y_ = float(transformed.y);
            vertex.z_ = float(transformed.z);
            vertex.normal_x_ = float(n.x);
            vertex.normal_y_ = float(n.y);
            vertex.normal_z_ = float(n.z);
            vertex.tangent_ = {float(t.x), float(t.y), float(t.z), 1};
        }
        auto reference = renderer.CreateMesh(expected, indices);
        std::array<ReadbackImage, 3> images;
        for (int mode = 0; mode < 3; ++mode) {
            SceneDrawList scene;
            scene.orthographic_ = true;
            scene.lights_ = {{{0, 0, 1}, {1, 0.7f, 0.5f}}};
            for (int copy = 0; copy < 2; ++copy) {
                MeshDraw draw;
                draw.mesh_ = mode == 2           ? reference.Handle()
                             : mode == 0 && copy ? alternate.Handle()
                                                 : mesh.Handle();
                draw.model_[0] = draw.model_[5] = draw.model_[10] = 0.45f;
                draw.normal_[0] = draw.normal_[5] = draw.normal_[10] = 1 / 0.45f;
                draw.model_[12] = copy ? 0.5f : -0.5f;
                draw.unlit_ = false;
                draw.double_sided_ = true;
                draw.textures_.slots_[1] = texture.Handle();
                if (mode != 2) draw.deformations_ = modifiers;
                scene.draws_.push_back(draw);
            }
            renderer.BeginFrame();
            renderer.SubmitScene(target.Handle(), scene);
            if (renderer.Stats().draws_ != (mode == 0 ? 2u : 1u))
                throw std::runtime_error("deformation.instance_batches");
            auto ticket = renderer.RequestReadback(target.Handle());
            renderer.EndFrame();
            images[mode] = Complete(renderer, std::move(ticket));
        }
        for (int mode = 0; mode < 2; ++mode) {
            std::uint64_t error = 0, visible = 0;
            for (std::size_t i = 0; i < images[mode].rgba_.size(); ++i) {
                error += std::abs(int(images[mode].rgba_[i]) - int(images[2].rgba_[i]));
                visible += i % 4 != 3 && images[mode].rgba_[i] > 20;
            }
            const auto mean = double(error) / images[mode].rgba_.size();
            if (visible < 100 || mean > 0.5)
                throw std::runtime_error("deformation.reference_pixels");
            std::cout << "deformation scenario=" << scenario << " mode=" << mode
                      << " mean_error=" << mean << '\n';
        }
    }
}
}  // namespace rhythm::validation
