// Test-only native boundary for P6.1. No runtime material format is adopted here.
#include <bgfx/bgfx.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <vector>

#include "bgfx_handles.h"
#include "gpu_execution_probe.h"
#include "rhythm/render/renderer.h"

namespace rhythm::validation {
namespace {
using render::detail::GpuHandle;
std::vector<std::uint8_t> ReadArtifact(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    const auto size = file.tellg();
    if (!file || size < 24 || size > 1024 * 1024)
        throw std::runtime_error("material_probe.artifact");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(bytes.data()), size);
    if (!file) throw std::runtime_error("material_probe.read");
    return bytes;
}
GpuHandle<bgfx::ShaderHandle> LoadShader(const std::filesystem::path& path) {
    const auto bytes = ReadArtifact(path);
    return GpuHandle(bgfx::createShader(bgfx::copy(bytes.data(), std::uint32_t(bytes.size()))));
}
}  // namespace

void VerifyMaterialProfile(std::span<std::uint8_t, 32 * 16 * 4> pixels,
                           const std::filesystem::path& directory) {
    // Caller retains readback bytes until after host/device destruction, even
    // when this test throws before the asynchronous transfer has completed.
    std::vector<GpuHandle<bgfx::ProgramHandle>> programs;
    constexpr std::array kVertices{"scene_vertex",       "scene_instance",
                                   "scene_skin_vertex",  "scene_skin_instance",
                                   "scene_morph_vertex", "scene_morph_instance"};
    for (const auto fragment_name :
         {"neutral", "animated_surface", "parameter_surface", "baseline"}) {
        auto fragment = LoadShader(directory / (std::string(fragment_name) + ".bin"));
        for (const auto name : kVertices) {
            auto vertex = LoadShader(directory / (std::string(name) + ".bin"));
            programs.emplace_back(bgfx::createProgram(vertex.Get(), fragment.Get(), false));
        }
    }
    constexpr std::array<float, 16> kIdentity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    // position, normal, UV, tangent: the actual scene vertex layout.
    constexpr std::array<float, 48> kPlane{
            -1, -1, .5f, 0, 0, 1, 0, 0, 1, 0, 0, 1, 1,  -1, .5f, 0, 0, 1, 1, 0, 1, 0, 0, 1,
            1,  1,  .5f, 0, 0, 1, 1, 1, 1, 0, 0, 1, -1, 1,  .5f, 0, 0, 1, 0, 1, 1, 0, 0, 1};
    constexpr std::array<std::uint16_t, 6> kIndices{0, 1, 2, 0, 2, 3};
    bgfx::VertexLayout layout;
    layout.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Tangent, 4, bgfx::AttribType::Float)
            .end();
    GpuHandle vertices(bgfx::createVertexBuffer(bgfx::copy(kPlane.data(), sizeof(kPlane)), layout));
    GpuHandle indices(bgfx::createIndexBuffer(bgfx::copy(kIndices.data(), sizeof(kIndices))));
    GpuHandle color(
            bgfx::createTexture2D(32, 16, false, 1, bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_RT));
    const auto attachment = color.Get();
    GpuHandle framebuffer(bgfx::createFrameBuffer(1, &attachment, false));
    GpuHandle staging(bgfx::createTexture2D(32, 16, false, 1, bgfx::TextureFormat::RGBA8,
                                            BGFX_TEXTURE_BLIT_DST | BGFX_TEXTURE_READ_BACK));
    std::map<std::string, GpuHandle<bgfx::UniformHandle>> uniforms;
    const auto set = [&](const char* name, std::array<float, 4> value) {
        if (!uniforms.contains(name))
            uniforms.emplace(name, GpuHandle(bgfx::createUniform(name, bgfx::UniformType::Vec4)));
        bgfx::setUniform(uniforms.at(name).Get(), value.data());
    };
    GpuHandle normal(bgfx::createUniform("u_scene_normal", bgfx::UniformType::Mat4));
    GpuHandle shadow_matrix(bgfx::createUniform("u_scene_shadow_matrix", bgfx::UniformType::Mat4));
    GpuHandle deform(bgfx::createUniform("u_scene_deform", bgfx::UniformType::Vec4, 4));
    GpuHandle pivot(bgfx::createUniform("u_scene_deform_pivot", bgfx::UniformType::Vec4, 4));
    constexpr std::array<float, 16> kZero{};
    // A complete texture binding set is required on GLES even in an unlit draw.
    constexpr std::uint32_t kWhite = 0xffffffff;
    GpuHandle white(bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8, 0,
                                          bgfx::copy(&kWhite, sizeof(kWhite))));
    std::vector<GpuHandle<bgfx::UniformHandle>> samplers;
    for (const auto name : {"s_scene_base", "s_scene_normal", "s_scene_orm", "s_scene_emission",
                            "s_scene_shadow", "s_scene_environment"})
        samplers.emplace_back(bgfx::createUniform(name, bgfx::UniformType::Sampler));
    const auto texture = [](std::array<std::uint8_t, 4> pixel) {
        return GpuHandle(bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8, 0,
                                               bgfx::copy(pixel.data(), 4)));
    };
    auto colored = texture({128, 64, 192, 255});
    auto tilted = texture({255, 128, 128, 255});
    auto blocked = texture({0, 0, 0, 255});
    std::vector<GpuHandle<bgfx::UniformHandle>> lights;
    for (const auto name :
         {"u_scene_light_directions", "u_scene_light_colors", "u_scene_light_positions",
          "u_scene_spot_directions", "u_scene_light_ranges"})
        lights.emplace_back(bgfx::createUniform(name, bgfx::UniformType::Vec4, 4));
    constexpr std::array kCases{"lit",      "base_srgb", "normal",     "orm",
                                "emission", "shadow",    "environment"};
    std::array<std::uint8_t, 32 * 16 * 4> baseline_pixels{};
    std::array<int, 3> plain{};
    for (int phase = 0; phase < 2 + 2 * int(kCases.size()); ++phase) {
        const bool tint_only = phase < 2;
        const auto scenario = tint_only ? -1 : (phase - 2) / 2;
        bgfx::setViewRect(0, 0, 0, 32, 16);
        bgfx::setViewFrameBuffer(0, framebuffer.Get());
        bgfx::setViewClear(0, BGFX_CLEAR_COLOR, 0x000000ff);
        bgfx::setViewTransform(0, kIdentity.data(), kIdentity.data());
        bgfx::setTransform(kIdentity.data());
        bgfx::setVertexBuffer(0, vertices.Get());
        bgfx::setIndexBuffer(indices.Get());
        bgfx::setUniform(normal.Get(), kIdentity.data());
        bgfx::setUniform(shadow_matrix.Get(), kIdentity.data());
        bgfx::setUniform(deform.Get(), kZero.data(), 4);
        bgfx::setUniform(pivot.Get(), kZero.data(), 4);
        set("u_scene_color", {1, 1, 1, 1});
        set("u_scene_material", {.5f, .5f, tint_only ? 1.0f : 0.0f, 1});
        set("u_scene_textures", {scenario == 1 ? 1.0f : 0.0f, scenario == 2 ? 1.0f : 0.0f,
                                 scenario == 3 ? 1.0f : 0.0f, scenario == 4 ? 1.0f : 0.0f});
        set("u_scene_texture_options", {1, 1, 0, 0});
        set("u_scene_emissive",
            scenario == 4 ? std::array<float, 4>{.4f, .4f, .4f, 0} : std::array<float, 4>{});
        set("u_scene_camera", {0, 0, 2, 1});
        set("u_scene_view", {0, 0, 1, 1});
        set("u_scene_shadow_settings", {scenario == 5 ? 0.0f : -1.0f, 0, 0, 1});
        set("u_scene_shadow_filter", {});
        set("u_scene_environment", {scenario == 6 ? .3f : 0.0f, 1, 0, 0});
        for (std::size_t index = 0; index < lights.size(); ++index) {
            auto values = kZero;
            if (index == 0) {
                values[0] = .6f;
                values[2] = .8f;
            }
            if (index == 1) values[0] = values[1] = values[2] = .5f;
            bgfx::setUniform(lights[index].Get(), values.data(), 4);
        }
        set("u_scene_uv", {1, 1, 0, 0});
        set("u_surface_params",
            phase ? std::array<float, 4>{0, 1, 0, 1} : std::array<float, 4>{1, 0, 0, 1});
        for (std::uint8_t index = 0; index < samplers.size(); ++index)
            bgfx::setTexture(index, samplers[index].Get(), white.Get());
        bgfx::setTexture(0, samplers[0].Get(), colored.Get());
        bgfx::setTexture(1, samplers[1].Get(), tilted.Get());
        bgfx::setTexture(2, samplers[2].Get(), colored.Get());
        bgfx::setTexture(3, samplers[3].Get(), colored.Get());
        bgfx::setTexture(4, samplers[4].Get(), blocked.Get());
        bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
        bgfx::submit(0, programs[tint_only ? 12 : (phase % 2 ? 0 : 18)].Get());
        bgfx::TextureRegion source, destination;
        source.init(color.Get());
        destination.init(staging.Get());
        bgfx::blit(1, destination, source);
        bgfx::frame();
        const auto ready = bgfx::read(destination, pixels.data());
        bool complete = false;
        for (int frame = 0; frame < 64; ++frame) {
            if (static_cast<std::int32_t>(bgfx::frame() - ready) >= 0) {
                complete = true;
                break;
            }
        }
        if (!complete) throw std::runtime_error("material_probe.readback_timeout");
        constexpr auto kCenter = (8 * 32 + 16) * 4;
        if (tint_only && (pixels[kCenter + phase] < 245 || pixels[kCenter + (1 - phase)] > 10 ||
                          pixels[kCenter + 2] > 10 || pixels[kCenter + 3] < 245))
            throw std::runtime_error("material_probe.tint_pixels");
        if (!tint_only) {
            if (phase % 2 == 0) {
                std::copy(pixels.begin(), pixels.end(), baseline_pixels.begin());
            } else {
                for (std::size_t index = 0; index < pixels.size(); ++index)
                    if (std::abs(int(pixels[index]) - int(baseline_pixels[index])) > 1)
                        throw std::runtime_error("material_probe.pbr_changed");
                if (scenario == 0) {
                    plain = {pixels[kCenter], pixels[kCenter + 1], pixels[kCenter + 2]};
                    if (plain[0] < 5) throw std::runtime_error("material_probe.unlit_control");
                } else {
                    int difference = 0;
                    for (std::size_t channel = 0; channel < plain.size(); ++channel)
                        difference += std::abs(int(pixels[kCenter + channel]) - plain[channel]);
                    if (difference < 5) throw std::runtime_error("material_probe.inactive_binding");
                }
                std::cout << "neutral expression preserves " << kCases[scenario] << " pixels\n";
            }
        }
        std::cout << "material phase=" << phase << " rgba=" << int(pixels[kCenter]) << ','
                  << int(pixels[kCenter + 1]) << ',' << int(pixels[kCenter + 2]) << ','
                  << int(pixels[kCenter + 3]) << '\n';
    }
    std::cout << "24 scene program pairs created; ordinary surface tint and 7 PBR binding controls "
                 "passed; "
                 "deformed geometry, generated shadows and runtime adoption remain separate\n";
}
void VerifySurfacePrograms(render::Renderer& renderer, const std::filesystem::path& directory) {
    using namespace render;
    const auto original_count = renderer.Stats().surface_programs_;
    auto artifact = ReadArtifact(directory / "parameter_surface.bin");
    auto program = renderer.CreateSurfaceProgram(artifact);
    auto neutral = renderer.CreateSurfaceProgram(ReadArtifact(directory / "neutral.bin"));
    const auto count = renderer.Stats().surface_programs_;
    artifact[4] = 0;
    bool rejected = false;
    try {
        renderer.CreateSurfaceProgram(artifact);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    if (!rejected || renderer.Stats().surface_programs_ != count ||
        !renderer.IsValid(program.Handle()))
        throw std::runtime_error("surface.atomic_admission");
    constexpr std::array<MeshVertex, 4> kPlane{
            {{-.35f, -.35f, 0}, {.35f, -.35f, 0}, {.35f, .35f, 0}, {-.35f, .35f, 0}}};
    constexpr std::array<std::uint32_t, 6> kIndices{0, 1, 2, 0, 2, 3};
    auto mesh = renderer.CreateMesh(kPlane, kIndices);
    auto target = renderer.CreateTexture({64, 64});
    SceneDrawList scene;
    for (float x : {-.5f, .5f}) {
        MeshDraw draw;
        draw.mesh_ = mesh.Handle();
        draw.model_[12] = x;
        draw.double_sided_ = true;
        draw.surface_program_ = SurfaceProgramInput{program.Handle(), {0, 1, 0, 1}};
        scene.draws_.push_back(draw);
    }
    for (int phase = 0; phase < 2; ++phase) {
        scene.draws_[0].surface_program_->parameters_ =
                phase ? std::array<float, 4>{0, 1, 0, 1} : std::array<float, 4>{1, 0, 0, 1};
        renderer.BeginFrame();
        renderer.SubmitScene(target.Handle(), scene);
        if (renderer.Stats().draws_ != (phase ? 1u : 2u))
            throw std::runtime_error("surface.instance_parameter_key");
        auto ticket = renderer.RequestReadback(target.Handle());
        renderer.EndFrame();
        std::optional<ReadbackImage> image;
        for (int wait = 0; wait < 64; ++wait) {
            image = ticket.Poll();
            if (image) break;
            renderer.BeginFrame();
            renderer.EndFrame();
        }
        if (!image) throw std::runtime_error("surface.readback_timeout");
        for (int side = 0; side < 2; ++side) {
            const auto offset = (32 * 64 + (side ? 48 : 16)) * 4;
            const bool green = phase || side;
            if (image->rgba_[offset + (green ? 1 : 0)] < 245 ||
                image->rgba_[offset + (green ? 0 : 1)] > 10)
                throw std::runtime_error("surface.parameter_pixels");
        }
    }
    const SurfaceProgramInput neutral_input{neutral.Handle()};
    VerifySceneInstances(renderer, neutral_input);
    VerifyMeshSkinning(renderer, neutral_input);
    VerifyMeshMorph(renderer, neutral_input);
    program = {};
    neutral = {};
    if (renderer.Stats().surface_programs_ != original_count)
        throw std::runtime_error("surface.resource_release");
    std::cout << "Surface Renderer: atomic admission, split/merged parameter draws, "
                 "instance/skin/morph pixels and release passed\n";
}
}  // namespace rhythm::validation
