#include "stable_shadow_gpu.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "rhythm/render/renderer.h"
#include "rhythm/scene/camera.h"
#include "rhythm/scene/math.h"
#include "shadow_pass.h"

namespace rhythm::validation {
namespace {
render::Matrix4 Matrix(const scene::Matrix& value) {
    render::Matrix4 result;
    for (std::size_t i = 0; i < result.size(); ++i) result[i] = float(value.values_[i]);
    return result;
}

std::vector<std::uint8_t> Capture(render::Renderer& renderer, render::TextureHandle output,
                                  render::TextureHandle shadow_color,
                                  render::TextureHandle shadow_depth,
                                  const scene::Camera& shadow_camera,
                                  render::SceneDrawList& occluders,
                                  render::SceneDrawList& receivers) {
    occluders.view_ = Matrix(scene::View(shadow_camera));
    occluders.projection_ = Matrix(scene::Projection(shadow_camera, 1));
    receivers.shadow_->world_to_clip_ = Matrix(
            scene::Multiply(scene::Projection(shadow_camera, 1), scene::View(shadow_camera)));
    renderer.BeginFrame();
    renderer.SubmitSceneDepth(shadow_color, shadow_depth, occluders);
    renderer.SubmitScene(output, receivers);
    auto ticket = renderer.RequestReadback(output);
    renderer.EndFrame();
    for (int i = 0; i < 32; ++i) {
        if (auto image = ticket.Poll()) return image->rgba_;
        renderer.BeginFrame();
        renderer.EndFrame();
    }
    throw std::runtime_error("shadow.stable_readback_timeout");
}

std::uint64_t RedDifference(const std::vector<std::uint8_t>& first,
                            const std::vector<std::uint8_t>& second) {
    if (first.size() != second.size()) throw std::runtime_error("shadow.stable_readback_size");
    std::uint64_t difference = 0;
    for (std::size_t i = 0; i < first.size(); i += 4)
        difference += first[i] > second[i] ? first[i] - second[i] : second[i] - first[i];
    return difference;
}
}  // namespace

void VerifyStableDirectionalShadows(render::Renderer& renderer) {
    using namespace render;
    const std::array<MeshVertex, 4> vertices{
            {{-0.95f, -0.95f}, {0.95f, -0.95f}, {0.95f, 0.95f}, {-0.95f, 0.95f}}};
    const std::array<std::uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
    auto mesh = renderer.CreateMesh(vertices, indices);
    auto shadow_color = renderer.CreateTexture({256, 256});
    auto shadow_depth = renderer.CreateDepthTexture({256, 256});
    auto output = renderer.CreateTexture({256, 256});

    SceneDrawList occluders;
    MeshDraw caster;
    caster.mesh_ = mesh.Handle();
    caster.double_sided_ = true;
    constexpr float kAngle = 0.37f;
    caster.model_[0] = std::cos(kAngle) * 0.24f;
    caster.model_[1] = std::sin(kAngle) * 0.24f;
    caster.model_[4] = -std::sin(kAngle) * 0.62f;
    caster.model_[5] = std::cos(kAngle) * 0.62f;
    caster.model_[12] = 0.08f;
    caster.model_[13] = 0.03f;
    caster.model_[14] = 0.5f;
    occluders.draws_ = {caster};

    SceneDrawList receivers;
    MeshDraw receiver;
    receiver.mesh_ = mesh.Handle();
    receiver.double_sided_ = true;
    receiver.unlit_ = false;
    receiver.roughness_ = 1;
    receivers.draws_ = {receiver};
    receivers.lights_ = {{{0, 0, 1}, {1, 0, 0}}};
    SceneShadow shadow;
    shadow.depth_ = shadow_depth.Handle();
    shadow.resolution_ = 256;
    shadow.filter_ = ShadowFilter::kPcf13;
    receivers.shadow_ = shadow;

    scene::Scene light_scene;
    light_scene.lights_.push_back({});
    light_scene.shadow_ = scene::ShadowSettings{};
    light_scene.shadow_->resolution_ = 256;
    light_scene.shadow_->extent_ = 2;
    light_scene.shadow_->distance_ = 2;
    const auto origin_camera = runtime::detail::ShadowCamera(light_scene);
    const auto origin = Capture(renderer, output.Handle(), shadow_color.Handle(),
                                shadow_depth.Handle(), origin_camera, occluders, receivers);

    const auto unit = light_scene.shadow_->extent_ * 2 / light_scene.shadow_->resolution_;
    const scene::Vector3 motion{unit * 0.49, unit * 0.31, 0};
    light_scene.shadow_->center_ = motion;
    const auto stable_camera = runtime::detail::ShadowCamera(light_scene);
    const auto stable = Capture(renderer, output.Handle(), shadow_color.Handle(),
                                shadow_depth.Handle(), stable_camera, occluders, receivers);
    const auto stable_difference = RedDifference(origin, stable);
    if (stable_difference != 0) throw std::runtime_error("shadow.stable_sub_grid_jitter");

    auto unsnapped_camera = origin_camera;
    unsnapped_camera.eye_ = motion;
    unsnapped_camera.eye_.z_ += light_scene.shadow_->distance_;
    unsnapped_camera.target_ = motion;
    const auto unsnapped = Capture(renderer, output.Handle(), shadow_color.Handle(),
                                   shadow_depth.Handle(), unsnapped_camera, occluders, receivers);
    const auto unsnapped_difference = RedDifference(origin, unsnapped);
    if (unsnapped_difference < 256)
        throw std::runtime_error("shadow.unsnapped_control_did_not_jitter");
    std::cout << "Stable directional shadows: sub-grid D3D11 output stayed identical; unsnapped "
                 "control red difference "
              << unsnapped_difference << "\n";
}
}  // namespace rhythm::validation
