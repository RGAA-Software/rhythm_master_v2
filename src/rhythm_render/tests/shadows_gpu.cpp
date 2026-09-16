#include <array>
#include <cstdint>
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
    auto output = renderer.CreateTexture({128, 128});
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
    constexpr auto kOccluded = (40 * 128 + 64) * 4;
    constexpr auto kLit = (88 * 128 + 64) * 4;
    receivers.shadow_->filter_ = ShadowFilter::kNearest;
    const auto nearest = capture();
    if (nearest[kOccluded] > 5 || nearest[kOccluded + 2] < 60 || nearest[kLit] < 60)
        throw std::runtime_error("shadow.position_direction_and_selected_light");
    if (nearest[kOccluded] > 5) throw std::runtime_error("shadow.hard_filter");
    receivers.shadow_->filter_ = ShadowFilter::kPcf5;
    const auto pcf5 = capture();
    receivers.shadow_->filter_ = ShadowFilter::kPcf13;
    const auto pcf13 = capture();
    const auto red_difference = [](const auto& first, const auto& second) {
        std::uint64_t difference = 0;
        for (std::size_t i = 0; i < first.size(); i += 4) {
            difference += first[i] > second[i] ? first[i] - second[i] : second[i] - first[i];
        }
        return difference;
    };
    const auto fractional_red = [](const auto& image) {
        std::size_t pixels = 0;
        for (std::size_t i = 0; i < image.size(); i += 4) {
            if (image[i] > 5 && image[i] < 60) ++pixels;
        }
        return pixels;
    };
    const auto nearest_to_pcf5 = red_difference(nearest, pcf5);
    const auto pcf5_to_pcf13 = red_difference(pcf5, pcf13);
    constexpr std::uint64_t kMinFilterEdgeDifference = 256;
    if (nearest_to_pcf5 < kMinFilterEdgeDifference)
        throw std::runtime_error("shadow.pcf5_changes_edge");
    if (pcf5_to_pcf13 < kMinFilterEdgeDifference)
        throw std::runtime_error("shadow.pcf13_changes_edge");
    const auto nearest_fractional = fractional_red(nearest);
    const auto pcf5_fractional = fractional_red(pcf5);
    const auto pcf13_fractional = fractional_red(pcf13);
    if (pcf5_fractional <= nearest_fractional) throw std::runtime_error("shadow.pcf5_softens_edge");
    if (pcf13_fractional <= pcf5_fractional) throw std::runtime_error("shadow.pcf13_softens_edge");
    receivers.shadow_->filter_ = ShadowFilter::kNearest;
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
    std::cout
            << "Shadows: sampled depth occlusion, Y orientation, selected light, bounds, bias and "
               "Nearest/PCF5/PCF13 edge differences passed ("
            << nearest_to_pcf5 << ", " << pcf5_to_pcf13 << "; fractional " << nearest_fractional
            << ", " << pcf5_fractional << ", " << pcf13_fractional << ")\n";
}
}  // namespace rhythm::validation
