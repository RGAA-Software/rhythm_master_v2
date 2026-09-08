#include <array>
#include <iostream>
#include <stdexcept>

#include "gpu_execution_probe.h"
#include "rhythm/render/renderer.h"

namespace rhythm::validation {
void VerifySceneShadows(render::Renderer& renderer) {
    using namespace render;
    const std::array<MeshVertex, 4> vertices{
            {{-0.95f, -0.95f}, {0.95f, -0.95f}, {0.95f, 0.95f}, {-0.95f, 0.95f}}};
    const std::array<std::uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
    auto mesh = renderer.CreateMesh(vertices, indices);
    auto shadow_color = renderer.CreateTexture({256, 256});
    auto shadow_depth = renderer.CreateDepthTexture({256, 256});
    auto output = renderer.CreateTexture({32, 32});
    SceneDrawList occluders;
    occluders.projection_[10] = -1;
    MeshDraw caster;
    caster.mesh_ = mesh.Handle();
    caster.double_sided_ = true;
    caster.model_[0] = 0.3f;
    caster.model_[5] = 0.2f;
    caster.model_[13] = 0.4f;
    caster.model_[14] = 0.5f;
    occluders.draws_ = {caster};
    SceneDrawList receivers;
    MeshDraw receiver;
    receiver.mesh_ = mesh.Handle();
    receiver.double_sided_ = true;
    receiver.unlit_ = false;
    receiver.roughness_ = 1;
    receivers.draws_ = {receiver};
    receivers.lights_ = {{{0, 0, 1}, {1, 0, 0}}, {{0, 0, 1}, {0, 0, 1}}};
    SceneShadow shadow;
    shadow.depth_ = shadow_depth.Handle();
    shadow.world_to_clip_[10] = -1;
    shadow.resolution_ = 256;
    receivers.shadow_ = shadow;
    const auto capture = [&] {
        renderer.BeginFrame();
        renderer.SubmitSceneDepth(shadow_color.Handle(), shadow_depth.Handle(), occluders);
        renderer.SubmitScene(output.Handle(), receivers);
        auto ticket = renderer.RequestReadback(output.Handle());
        renderer.EndFrame();
        for (int i = 0; i < 32; ++i) {
            if (auto image = ticket.Poll()) return image->rgba_;
            renderer.BeginFrame();
            renderer.EndFrame();
        }
        throw std::runtime_error("shadow.readback_timeout");
    };
    constexpr auto kOccluded = (10 * 32 + 16) * 4;
    constexpr auto kLit = (22 * 32 + 16) * 4;
    const auto first = capture();
    if (first[kOccluded] > 5 || first[kOccluded + 2] < 60 || first[kLit] < 60)
        throw std::runtime_error("shadow.position_direction_and_selected_light");
    receivers.shadow_->filter_ = false;
    if (capture()[kOccluded] > 5) throw std::runtime_error("shadow.hard_filter");
    occluders.draws_[0].model_[14] = -0.5f;
    if (capture()[kOccluded] < 60) throw std::runtime_error("shadow.behind_receiver");
    occluders.draws_[0].model_[14] = 0.5f;
    receivers.shadow_->world_to_clip_[12] = 3;
    if (capture()[kOccluded] < 60) throw std::runtime_error("shadow.outside_volume_is_lit");
    receivers.shadow_->world_to_clip_[12] = 0;
    occluders.draws_[0].model_[14] = 0.002f;
    receivers.shadow_->depth_bias_ = 0.002f;
    if (capture()[kOccluded] < 60) throw std::runtime_error("shadow.bias_prevents_acne");
    // Perspective spot projection uses clip W as well as the mapped depth.
    occluders.view_[14] = -3;
    occluders.projection_ = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, -1.020202f, -1, 0, 0, -0.2020202f, 0};
    occluders.draws_[0].model_[14] = 0.5f;
    receivers.lights_.clear();
    PositionalLight spot;
    spot.spot_ = true;
    spot.radiance_ = {9, 0, 0};
    spot.range_ = 100;
    spot.cone_angle_ = 45;
    receivers.positional_lights_ = {spot};
    receivers.shadow_->world_to_clip_ = occluders.projection_;
    receivers.shadow_->world_to_clip_[14] = 2.858586f;
    receivers.shadow_->world_to_clip_[15] = 3;
    const auto perspective = capture();
    if (perspective[kOccluded] > 5 || perspective[kLit] < 50)
        throw std::runtime_error("shadow.perspective_spot");
    receivers.shadow_.reset();
    if (capture()[kOccluded] < 60) throw std::runtime_error("shadow.disable_releases_binding");
    std::cout << "Shadows: sampled depth occlusion, Y orientation, selected light, bounds and bias "
                 "passed\n";
}
}  // namespace rhythm::validation
