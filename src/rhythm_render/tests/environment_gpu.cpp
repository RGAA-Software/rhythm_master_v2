#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>

#include "gpu_execution_probe.h"
#include "rhythm/render/renderer.h"

namespace rhythm::validation {
namespace {
render::DrawList EnvironmentQuad(render::TextureHandle source) {
    render::DrawList list;
    list.width_ = 780;
    list.height_ = 66;
    list.vertices_ = {{0, 0, 0, 0}, {780, 0, 1, 0}, {780, 66, 1, 1}, {0, 66, 0, 1}};
    list.indices_ = {0, 1, 2, 0, 2, 3};
    list.commands_ = {{source, 0, 6, {0, 0, 780, 66}}};
    return list;
}
render::DrawList TextureQuad(render::TextureHandle source, render::Extent extent) {
    render::DrawList list;
    list.width_ = extent.width_;
    list.height_ = extent.height_;
    list.vertices_ = {{0, 0, 0, 0},
                      {static_cast<float>(extent.width_), 0, 1, 0},
                      {static_cast<float>(extent.width_), static_cast<float>(extent.height_), 1, 1},
                      {0, static_cast<float>(extent.height_), 0, 1}};
    list.indices_ = {0, 1, 2, 0, 2, 3};
    list.commands_ = {
            {source,
             0,
             6,
             {0, 0, static_cast<float>(extent.width_), static_cast<float>(extent.height_)}}};
    return list;
}
render::ReadbackImage EnvironmentComplete(render::Renderer& renderer, render::Readback ticket) {
    for (int i = 0; i < 32; ++i) {
        if (auto result = ticket.Poll()) return std::move(*result);
        renderer.BeginFrame();
        renderer.EndFrame();
    }
    throw std::runtime_error("environment.readback_timeout");
}
}  // namespace
void VerifyEnvironmentLighting(render::Renderer& renderer) {
    using namespace render;
    const std::array<std::uint8_t, 4> gray{128, 128, 128, 128}, occluded{0, 255, 255, 255};
    auto source = renderer.CreateTexture({1, 1}, gray);
    auto ao = renderer.CreateTexture({1, 1}, occluded);
    auto atlas = renderer.CreateTexture(kEnvironmentAtlasExtent, {}, TexturePrecision::kFloat16);
    auto inspection = renderer.CreateTexture(kEnvironmentAtlasExtent);
    auto output = renderer.CreateTexture({32, 32});
    auto prepare = EnvironmentQuad(source.Handle());
    prepare.commands_[0].environment_filter_ = EnvironmentFilter{false};
    auto inspect = EnvironmentQuad(atlas.Handle());
    const auto atlas_pixels = [&] {
        renderer.BeginFrame();
        renderer.Submit(atlas.Handle(), prepare);
        renderer.Submit(inspection.Handle(), inspect);
        auto ticket = renderer.RequestReadback(inspection.Handle());
        renderer.EndFrame();
        return EnvironmentComplete(renderer, std::move(ticket));
    };
    for (bool srgb : {false, true}) {
        prepare.commands_[0].environment_filter_->source_srgb_ = srgb;
        const auto image = atlas_pixels();
        // Constant energy survives every convolution level, including both gutters.
        for (std::size_t i = 0; i < image.rgba_.size(); i += 4)
            if (std::abs(int(image.rgba_[i]) - (srgb ? 55 : 128)) > 2 || image.rgba_[i + 3] != 255)
                throw std::runtime_error("environment.constant_convolution_transfer_alpha");
    }
    // A floating source carries radiance above one through every filter tile.
    auto hdr = renderer.CreateTexture({16, 8}, {}, TexturePrecision::kFloat16);
    auto amplify = EnvironmentQuad(source.Handle());
    amplify.commands_[0].color_pipeline_ =
            ColorPipeline{ColorTransfer::kLinear, ColorTransfer::kLinear, ToneMapping::kNone, 2};
    renderer.BeginFrame();
    renderer.Submit(hdr.Handle(), amplify);
    renderer.EndFrame();
    prepare.commands_[0].texture_ = hdr.Handle();
    prepare.commands_[0].environment_filter_->source_srgb_ = false;
    inspect.commands_[0].color_pipeline_ =
            ColorPipeline{ColorTransfer::kLinear, ColorTransfer::kLinear, ToneMapping::kNone, -2};
    const auto floating = atlas_pixels();
    for (std::size_t i = 0; i < floating.rgba_.size(); i += 4)
        if (std::abs(int(floating.rgba_[i]) - 128) > 2)
            throw std::runtime_error("environment.hdr_radiance_preserved");
    prepare.commands_[0].texture_ = source.Handle();
    const std::array<MeshVertex, 4> vertices{
            {{-0.95f, -0.95f}, {0.95f, -0.95f}, {0.95f, 0.95f}, {-0.95f, 0.95f}}};
    const std::array<std::uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
    auto mesh = renderer.CreateMesh(vertices, indices);
    SceneDrawList scene;
    MeshDraw draw;
    draw.mesh_ = mesh.Handle();
    draw.unlit_ = false;
    draw.double_sided_ = true;
    draw.metallic_ = 1;
    draw.roughness_ = 0;
    scene.draws_ = {draw};
    scene.orthographic_ = true;
    scene.environment_ = SceneEnvironment{atlas.Handle()};
    const auto capture = [&] {
        renderer.BeginFrame();
        renderer.Submit(atlas.Handle(), prepare);
        renderer.SubmitScene(output.Handle(), scene);
        auto ticket = renderer.RequestReadback(output.Handle());
        renderer.EndFrame();
        const auto image = EnvironmentComplete(renderer, std::move(ticket));
        constexpr auto kCenter = (16 * 32 + 16) * 4;
        return std::array{image.rgba_[kCenter], image.rgba_[kCenter + 1], image.rgba_[kCenter + 2]};
    };
    prepare.commands_[0].environment_filter_->source_srgb_ = false;
    if (std::abs(int(capture()[0]) - 128) > 3)
        throw std::runtime_error("environment.metal_reflection");
    scene.draws_[0].roughness_ = 1;
    if (std::abs(int(capture()[0]) - 60) > 3) throw std::runtime_error("environment.rough_brdf");
    scene.draws_[0].metallic_ = 0;
    if (capture()[0] < 128) throw std::runtime_error("environment.diffuse_convolution");
    scene.draws_[0].textures_.slots_[2] = ao.Handle();
    if (capture()[0] != 0) throw std::runtime_error("environment.occlusion");
    scene.lights_ = {{{0, 0, 1}, {1, 1, 1}}};
    if (capture()[0] < 60) throw std::runtime_error("environment.ao_preserves_direct_light");
    scene.lights_.clear();
    scene.draws_[0] = draw;

    // A fixed linear HDR highlight must broaden and lose peak energy smoothly
    // across the five Godot perceptual-roughness filter levels.
    constexpr Extent kHdrExtent{128, 64};
    std::vector<std::uint8_t> hdr_pixels(kHdrExtent.width_ * kHdrExtent.height_ * 4, 0);
    for (std::uint32_t y = 0; y < kHdrExtent.height_; ++y)
        for (std::uint32_t x = 0; x < kHdrExtent.width_; ++x) {
            const auto offset = (y * kHdrExtent.width_ + x) * 4;
            const auto radiance = static_cast<std::uint8_t>(x >= 94 && x <= 98 ? 48 : 2);
            hdr_pixels[offset] = hdr_pixels[offset + 1] = hdr_pixels[offset + 2] = radiance;
            hdr_pixels[offset + 3] = 255;
        }
    auto hdr_source = renderer.CreateTexture(kHdrExtent, hdr_pixels);
    auto hdr_linear = renderer.CreateTexture(kHdrExtent, {}, TexturePrecision::kFloat16);
    auto make_hdr = TextureQuad(hdr_source.Handle(), kHdrExtent);
    make_hdr.commands_[0].color_pipeline_ =
            ColorPipeline{ColorTransfer::kLinear, ColorTransfer::kLinear, ToneMapping::kNone, 2};
    renderer.BeginFrame();
    renderer.Submit(hdr_linear.Handle(), make_hdr);
    renderer.EndFrame();
    prepare.commands_[0].texture_ = hdr_linear.Handle();
    std::array<std::uint8_t, 5> roughness_ladder{};
    const std::array<float, 5> roughness{0, 0.0625f, 0.25f, 0.5625f, 1};
    for (std::size_t i = 0; i < roughness.size(); ++i) {
        scene.draws_[0].roughness_ = roughness[i];
        roughness_ladder[i] = capture()[0];
    }
    constexpr std::array<int, 5> kExpectedLadder{192, 185, 110, 22, 9};
    for (std::size_t i = 0; i < roughness_ladder.size(); ++i) {
        if (std::abs(int(roughness_ladder[i]) - kExpectedLadder[i]) > 8 ||
            (i > 0 && roughness_ladder[i] >= roughness_ladder[i - 1]))
            throw std::runtime_error("environment.hdr_roughness_ladder");
    }
    std::cout << "Environment HDR roughness ladder:";
    for (const auto value : roughness_ladder) std::cout << ' ' << int(value);
    std::cout << '\n';

    std::vector<std::uint8_t> hemispheres(64 * 32 * 4, 0);
    for (int y = 0; y < 32; ++y)
        for (int x = 0; x < 64; ++x) {
            const auto offset = (y * 64 + x) * 4;
            hemispheres[offset + (x < 32 ? 0 : 2)] = 200;
            hemispheres[offset + 3] = 255;
        }
    auto directional = renderer.CreateTexture({64, 32}, hemispheres);
    prepare.commands_[0].texture_ = directional.Handle();
    scene.draws_[0].roughness_ = 0;
    const auto blue = capture();
    scene.environment_->rotation_ = 180;
    const auto red = capture();
    if (blue[2] < 180 || blue[0] > 5 || red[0] < 180 || red[2] > 5)
        throw std::runtime_error("environment.rotation_and_longitude");
    scene.environment_.reset();
    if (capture()[0] != 0) throw std::runtime_error("environment.disabled");
    std::cout << "Environment: GGX/diffuse atlas, gutters, color transfer, metal/roughness, AO and "
                 "rotation passed\n";
}
}  // namespace rhythm::validation
