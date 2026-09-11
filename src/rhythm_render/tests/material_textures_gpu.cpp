#include <array>
#include <iostream>
#include <stdexcept>

#include "gpu_execution_probe.h"
#include "rhythm/render/renderer.h"

namespace rhythm::validation {
void VerifyMaterialTextures(render::Renderer& renderer) {
    using namespace render;
    const std::array<MeshVertex, 4> vertices{{{-1, -1, 0, 0, 0, 1, 0, 1},
                                              {1, -1, 0, 0, 0, 1, 1, 1},
                                              {1, 1, 0, 0, 0, 1, 1, 0},
                                              {-1, 1, 0, 0, 0, 1, 0, 0}}};
    const std::array<std::uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
    auto mesh = renderer.CreateMesh(vertices, indices);
    auto target = renderer.CreateTexture({32, 32});
    const std::array<std::uint8_t, 4> gray{128, 128, 128, 128};
    const std::array<std::uint8_t, 4> sideways{255, 128, 128, 255};
    const std::array<std::uint8_t, 4> metal{255, 255, 255, 255};
    const std::array<std::uint8_t, 4> dielectric{255, 255, 0, 255};
    auto gray_texture = renderer.CreateTexture({1, 1}, gray);
    auto normal_texture = renderer.CreateTexture({1, 1}, sideways);
    auto metal_texture = renderer.CreateTexture({1, 1}, metal);
    auto dielectric_texture = renderer.CreateTexture({1, 1}, dielectric);
    SceneDrawList scene;
    MeshDraw draw;
    draw.mesh_ = mesh.Handle();
    draw.double_sided_ = true;
    draw.textures_.slots_[0] = gray_texture.Handle();
    scene.draws_ = {draw};
    std::uint32_t submissions = 0;
    const auto capture = [&] {
        scene.draws_[0] = draw;
        renderer.BeginFrame();
        renderer.SubmitScene(target.Handle(), scene);
        submissions = renderer.Stats().draws_;
        auto ticket = renderer.RequestReadback(target.Handle());
        renderer.EndFrame();
        for (int i = 0; i < 32; ++i) {
            if (auto image = ticket.Poll()) return image->rgba_;
            renderer.BeginFrame();
            renderer.EndFrame();
        }
        throw std::runtime_error("material.readback_timeout");
    };
    const auto center = [&] { return int(capture()[(16 * 32 + 16) * 4]); };
    const auto linear = center();
    draw.textures_.color_srgb_ = false;
    const auto raw = center();
    if (linear < 53 || linear > 57 || raw < 126 || raw > 130)
        throw std::runtime_error("material.premultiplied_color_transfer");
    std::array<std::uint8_t, 8 * 8 * 4> grid{};
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x) {
            const auto index = (y * 8 + x) * 4;
            grid[index + (y < 4 ? (x < 4 ? 0 : 1) : 2)] = 255;
            grid[index + 3] = 255;
        }
    auto grid_texture = renderer.CreateTexture({8, 8}, grid);
    draw.textures_.slots_[0] = grid_texture.Handle();
    const auto image = capture();
    if (image[(8 * 32 + 8) * 4] < 250 || image[(8 * 32 + 24) * 4 + 1] < 250 ||
        image[(24 * 32 + 8) * 4 + 2] < 250)
        throw std::runtime_error("material.uv_top_left");
    draw.textures_.slots_[0] = {};
    draw.unlit_ = false;
    draw.roughness_ = 1;
    scene.lights_ = {{{0, 0, 1}, {1, 1, 1}}};
    const auto flat = center();
    draw.textures_.slots_[1] = normal_texture.Handle();
    const auto tilted = center();
    scene.lights_[0].direction_ = {1, 0, 0};
    const auto side = center();
    if (flat < 60 || tilted > 5 || side < 60)
        throw std::runtime_error("material.tangent_normal_direction");
    draw.model_[0] = -2;
    draw.model_[5] = 0.5f;
    draw.normal_[0] = -0.5f;
    draw.normal_[5] = 2;
    const auto mirrored = center();
    scene.lights_[0].direction_ = {-1, 0, 0};
    if (mirrored > 5 || center() < 60)
        throw std::runtime_error("material.mirrored_nonuniform_tangent");
    draw.model_ = draw.normal_ = kIdentityMatrix;
    draw.textures_.slots_[1] = {};
    draw.textures_.slots_[2] = dielectric_texture.Handle();
    draw.metallic_ = 1;
    scene.lights_[0].direction_ = {0, 0, 1};
    const auto nonmetal = center();
    draw.textures_.slots_[2] = metal_texture.Handle();
    if (nonmetal < 60 || center() >= nonmetal - 10)
        throw std::runtime_error("material.orm_metallic_channel");
    scene.lights_.clear();
    draw.emissive_ = {1, 1, 1};
    draw.textures_.slots_[3] = gray_texture.Handle();
    draw.textures_.color_srgb_ = true;
    if (center() < 53 || center() > 57) throw std::runtime_error("material.emission_transfer");
    const std::array<std::uint8_t, 4> red{255, 0, 0, 255}, blue{0, 0, 255, 255};
    auto red_texture = renderer.CreateTexture({1, 1}, red);
    auto blue_texture = renderer.CreateTexture({1, 1}, blue);
    draw = {};
    draw.mesh_ = mesh.Handle();
    draw.double_sided_ = true;
    draw.model_[0] = 0.4f;
    draw.model_[12] = -0.5f;
    draw.textures_.slots_[0] = red_texture.Handle();
    auto second = draw;
    second.model_[12] = 0.5f;
    second.textures_.slots_[0] = blue_texture.Handle();
    scene.draws_.push_back(second);
    const auto separate = capture();
    if (submissions != 2 || separate[(16 * 32 + 8) * 4] < 250 ||
        separate[(16 * 32 + 24) * 4 + 2] < 250)
        throw std::runtime_error("material.incompatible_textures_do_not_batch");
    scene.draws_[1].textures_ = draw.textures_;
    const auto batched = capture();
    if (submissions != 1 || batched[(16 * 32 + 8) * 4] < 250 || batched[(16 * 32 + 24) * 4] < 250)
        throw std::runtime_error("material.compatible_textures_batch");
    std::array<std::uint8_t, 8 * 4> coverage{};
    for (int x = 0; x < 8; ++x) {
        coverage[x * 4] = 255;
        coverage[x * 4 + 3] = x < 4 ? 255 : 0;
    }
    auto coverage_texture = renderer.CreateTexture({8, 1}, coverage);
    draw = {};
    draw.mesh_ = mesh.Handle();
    draw.double_sided_ = true;
    draw.color_ = {0, 0, 1, 1};
    draw.model_[14] = 0.5f;
    auto foreground = draw;
    foreground.color_ = {1, 0, 0, 1};
    foreground.model_[14] = -0.5f;
    foreground.alpha_depth_prepass_ = true;
    foreground.textures_.slots_[0] = coverage_texture.Handle();
    auto middle = draw;
    middle.color_ = {0, 1, 0, 0.5f};
    middle.model_[14] = 0;
    scene.draws_ = {draw, foreground, middle};
    const auto prepass = capture();
    const auto left = (16 * 32 + 4) * 4;
    const auto right = (16 * 32 + 27) * 4;
    if (submissions != 4 || prepass[left] < 250 || prepass[left + 1] > 3 || prepass[left + 2] > 3 ||
        prepass[right] > 3 || prepass[right + 1] < 125 || prepass[right + 2] < 125)
        throw std::runtime_error("material.alpha_depth_prepass_coverage");
    std::cout << "Material textures: color transfer, UV, normal, mirrored scale, ORM and emission "
                 "and alpha depth prepass passed\n";
}
}  // namespace rhythm::validation
