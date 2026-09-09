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
render::ReadbackImage CompleteSkinReadback(render::Renderer& renderer, render::Readback ticket) {
    for (int i = 0; i < 32; ++i) {
        if (auto image = ticket.Poll()) return std::move(*image);
        renderer.BeginFrame();
        renderer.EndFrame();
    }
    throw std::runtime_error("skin.readback_timeout");
}
}  // namespace
void VerifyMeshSkinning(render::Renderer& renderer,
                        std::optional<render::SurfaceProgramInput> surface) {
    using namespace render;
    std::vector<MeshVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<SkinWeights> weights;
    constexpr std::uint32_t kSegments = 20;
    for (std::uint32_t y = 0; y <= kSegments; ++y)
        for (std::uint32_t x = 0; x <= kSegments; ++x) {
            const float u = float(x) / kSegments, v = float(y) / kSegments;
            vertices.push_back({(u - 0.5f) * 1.5f, (v - 0.5f) * 1.5f, 0, 0, 0, 1, u, v});
            weights.push_back({{0, 1, 46, 47}, {u * 0.4f, (1 - u) * 0.4f, 0.2f, 0.4f}});
            if (x < kSegments && y < kSegments) {
                const auto a = y * (kSegments + 1) + x, b = a + kSegments + 1;
                indices.insert(indices.end(), {a, a + 1, b, b, a + 1, b + 1});
            }
        }
    auto mesh = renderer.CreateMesh(vertices, indices, weights);
    auto alternate = renderer.CreateMesh(vertices, indices, weights);
    auto target = renderer.CreateTexture({128, 128});
    const std::array<std::uint8_t, 4> tilted_normal{180, 156, 238, 255};
    auto texture = renderer.CreateTexture({1, 1}, tilted_normal);
    for (int scenario = 0; scenario < 4; ++scenario) {
        std::vector<Matrix4> bones(kMaximumSkinBones, kIdentityMatrix);
        for (std::size_t i = 0; i < bones.size(); ++i) {
            auto matrix = glm::mat4(1);
            if (scenario != 0) {
                matrix = glm::translate(matrix, {0.04f * float(i % 3), 0.03f * float(i % 5), 0});
                matrix = glm::rotate(matrix, 0.005f * float(i * static_cast<std::size_t>(scenario)),
                                     glm::normalize(glm::vec3(1, 2, 3)));
                if (scenario >= 2) matrix = glm::scale(matrix, {0.8f, 1.15f, 0.9f});
            }
            std::copy_n(glm::value_ptr(matrix), 16, bones[i].begin());
        }
        auto expected = vertices;
        for (std::size_t i = 0; i < expected.size(); ++i) {
            glm::mat4 matrix(0);
            for (std::size_t j = 0; j < 4; ++j)
                matrix += glm::make_mat4(bones[weights[i].joints_[j]].data()) *
                          weights[i].weights_[j];
            auto& vertex = expected[i];
            const auto p = matrix * glm::vec4(vertex.x_, vertex.y_, vertex.z_, 1);
            const auto n = glm::normalize(glm::transpose(glm::inverse(glm::mat3(matrix))) *
                                          glm::vec3(0, 0, 1));
            const auto t = glm::normalize(glm::mat3(matrix) * glm::vec3(1, 0, 0));
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
                draw.surface_program_ = surface;
                draw.mesh_ = mode == 2           ? reference.Handle()
                             : mode == 0 && copy ? alternate.Handle()
                                                 : mesh.Handle();
                draw.model_[0] = draw.model_[5] = draw.model_[10] = 0.45f;
                draw.normal_[0] = draw.normal_[5] = draw.normal_[10] = 1 / 0.45f;
                draw.model_[12] = copy ? 0.5f : -0.5f;
                draw.unlit_ = false;
                draw.double_sided_ = true;
                draw.textures_.slots_[1] = texture.Handle();
                if (mode != 2) draw.bones_ = bones;
                scene.draws_.push_back(std::move(draw));
            }
            renderer.BeginFrame();
            renderer.SubmitScene(target.Handle(), scene);
            if (renderer.Stats().draws_ != (mode == 0 ? 2u : 1u))
                throw std::runtime_error("skin.instance_batches");
            auto ticket = renderer.RequestReadback(target.Handle());
            renderer.EndFrame();
            images[mode] = CompleteSkinReadback(renderer, std::move(ticket));
        }
        for (int mode = 0; mode < 2; ++mode) {
            std::uint64_t error = 0, visible = 0;
            for (std::size_t i = 0; i < images[mode].rgba_.size(); ++i) {
                error += std::abs(int(images[mode].rgba_[i]) - int(images[2].rgba_[i]));
                visible += i % 4 != 3 && images[mode].rgba_[i] > 20;
            }
            const auto mean = double(error) / images[mode].rgba_.size();
            std::cout << "skin bones=48 scenario=" << scenario << " mode=" << mode
                      << " mean_error=" << mean << '\n';
            if (visible < 100 || mean > 0.5) throw std::runtime_error("skin.reference_pixels");
        }
    }
}
}  // namespace rhythm::validation
