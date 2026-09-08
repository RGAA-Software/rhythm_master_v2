#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <stdexcept>

#include "gpu_execution_probe.h"
#include "rhythm/render/renderer.h"

namespace rhythm::validation {
namespace {
render::ReadbackImage CompleteMorphReadback(render::Renderer& renderer, render::Readback ticket) {
    for (int i = 0; i < 32; ++i) {
        if (auto image = ticket.Poll()) return std::move(*image);
        renderer.BeginFrame();
        renderer.EndFrame();
    }
    throw std::runtime_error("morph.readback_timeout");
}
}  // namespace
void VerifyMeshMorph(render::Renderer& renderer) {
    using namespace render;
    std::vector<MeshVertex> vertices;
    std::vector<std::uint32_t> indices;
    constexpr std::uint32_t kSegments = 60;
    for (std::uint32_t y = 0; y <= kSegments; ++y)
        for (std::uint32_t x = 0; x <= kSegments; ++x) {
            const float u = float(x) / kSegments, v = float(y) / kSegments;
            vertices.push_back({(u - 0.5f) * 1.5f, (v - 0.5f) * 1.5f, 0, 0, 0, 1, u, v});
            if (x < kSegments && y < kSegments) {
                const auto a = y * (kSegments + 1) + x, b = a + kSegments + 1;
                indices.insert(indices.end(), {a, a + 1, b, b, a + 1, b + 1});
            }
        }
    std::array<MorphTarget, 4> targets;
    for (std::size_t target = 0; target < targets.size(); ++target)
        for (const auto& vertex : vertices) {
            const float phase = float(target + 1) * 0.7f;
            const float z = std::sin(vertex.x_ * 3 + phase) * 0.2f;
            const float dz = std::cos(vertex.x_ * 3 + phase) * 0.6f;
            const auto n = glm::normalize(glm::vec3(-dz, 0, 1));
            const auto t = glm::normalize(glm::vec3(1, 0, dz));
            targets[target].deltas_.push_back(
                    {{0.08f * float(target), 0, z}, {n.x, n.y, n.z - 1}, {t.x - 1, t.y, t.z}});
        }
    // The last target exceeds lowp sampler range and spans many texture rows.
    for (auto& delta : targets[3].deltas_) delta.position_[0] += 3;
    std::vector<SkinWeights> skin(vertices.size());
    for (auto& vertex : skin) vertex.joints_.fill(47);
    auto mesh = renderer.CreateMesh(vertices, indices, {}, targets);
    auto alternate = renderer.CreateMesh(vertices, indices, {}, targets);
    auto skinned = renderer.CreateMesh(vertices, indices, skin, targets);
    auto skinned_alternate = renderer.CreateMesh(vertices, indices, skin, targets);
    auto target = renderer.CreateTexture({128, 128});
    const std::array<std::uint8_t, 4> tilted_normal{180, 156, 238, 255};
    auto texture = renderer.CreateTexture({1, 1}, tilted_normal);
    for (int scenario = 0; scenario < 4; ++scenario) {
        const std::array<float, 4> weights =
                scenario == 0   ? std::array<float, 4>{}
                : scenario == 3 ? std::array<float, 4>{0.5f, -0.2f, 0.7f, -0.05f}
                                : std::array<float, 4>{0.3f, 0.2f, 0.4f, 0.04f};
        auto bone = glm::rotate(glm::mat4(1), 0.2f, glm::vec3(0, 1, 0));
        bone = glm::scale(bone, {0.8f, 1.1f, 0.9f});
        std::vector<Matrix4> palette(48, kIdentityMatrix);
        std::copy_n(glm::value_ptr(bone), 16, palette[47].begin());
        const bool use_skin = scenario >= 2;
        auto expected = vertices;
        for (std::size_t i = 0; i < expected.size(); ++i) {
            auto& vertex = expected[i];
            glm::vec3 p(vertex.x_, vertex.y_, vertex.z_), n(0, 0, 1), t(1, 0, 0);
            for (std::size_t j = 0; j < 4; ++j) {
                const auto& delta = targets[j].deltas_[i];
                p += glm::make_vec3(delta.position_.data()) * weights[j];
                n += glm::make_vec3(delta.normal_.data()) * weights[j];
                t += glm::make_vec3(delta.tangent_.data()) * weights[j];
            }
            n = glm::normalize(n);
            t = glm::normalize(t);
            if (use_skin) {
                p = glm::vec3(bone * glm::vec4(p, 1));
                n = glm::normalize(glm::transpose(glm::inverse(glm::mat3(bone))) * n);
                t = glm::normalize(glm::mat3(bone) * t);
            }
            t = glm::normalize(t - n * glm::dot(n, t));
            vertex.x_ = p.x;
            vertex.y_ = p.y;
            vertex.z_ = p.z;
            vertex.normal_x_ = n.x;
            vertex.normal_y_ = n.y;
            vertex.normal_z_ = n.z;
            vertex.tangent_ = {t.x, t.y, t.z, 1};
        }
        auto reference = renderer.CreateMesh(expected, indices);
        std::array<ReadbackImage, 3> images;
        for (int mode = 0; mode < 3; ++mode) {
            SceneDrawList scene;
            scene.orthographic_ = true;
            scene.lights_ = {{{0, 0, 1}, {1, 0.7f, 0.5f}}};
            for (int copy = 0; copy < 2; ++copy) {
                MeshDraw draw;
                draw.mesh_ = mode == 2  ? reference.Handle()
                             : use_skin ? (mode == 0 && copy ? skinned_alternate.Handle()
                                                             : skinned.Handle())
                                        : (mode == 0 && copy ? alternate.Handle() : mesh.Handle());
                draw.model_[0] = draw.model_[5] = draw.model_[10] = 0.45f;
                draw.normal_[0] = draw.normal_[5] = draw.normal_[10] = 1 / 0.45f;
                draw.model_[12] = copy ? 0.5f : -0.5f;
                draw.unlit_ = false;
                draw.double_sided_ = true;
                draw.textures_.slots_[1] = texture.Handle();
                if (mode != 2) {
                    draw.morph_weights_ = weights;
                    if (use_skin) draw.bones_ = palette;
                }
                scene.draws_.push_back(std::move(draw));
            }
            renderer.BeginFrame();
            renderer.SubmitScene(target.Handle(), scene);
            if (renderer.Stats().draws_ != (mode == 0 ? 2u : 1u))
                throw std::runtime_error("morph.instance_batches");
            auto ticket = renderer.RequestReadback(target.Handle());
            renderer.EndFrame();
            images[mode] = CompleteMorphReadback(renderer, std::move(ticket));
        }
        for (int mode = 0; mode < 2; ++mode) {
            std::uint64_t error = 0, visible = 0;
            for (std::size_t i = 0; i < images[mode].rgba_.size(); ++i) {
                error += std::abs(int(images[mode].rgba_[i]) - int(images[2].rgba_[i]));
                visible += i % 4 != 3 && images[mode].rgba_[i] > 20;
            }
            const auto mean = double(error) / images[mode].rgba_.size();
            std::cout << "morph targets=4 vertices=" << vertices.size() << " scenario=" << scenario
                      << " mode=" << mode << " mean_error=" << mean << '\n';
            if (visible < 100 || mean > 0.5) throw std::runtime_error("morph.reference_pixels");
        }
    }
}
}  // namespace rhythm::validation
