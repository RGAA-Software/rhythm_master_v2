// Test-only native boundary for P6.1. No runtime material format is adopted here.
#include <bgfx/bgfx.h>

#include <array>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <vector>

#include "bgfx_handles.h"
#include "gpu_execution_probe.h"

namespace rhythm::validation {
namespace {
using render::detail::GpuHandle;
GpuHandle<bgfx::ShaderHandle> LoadShader(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    const auto size = file.tellg();
    if (!file || size < 24 || size > 1024 * 1024)
        throw std::runtime_error("material_probe.artifact");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(bytes.data()), size);
    if (!file) throw std::runtime_error("material_probe.read");
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
    for (const auto fragment_name : {"neutral", "animated_surface", "parameter_surface"}) {
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
    for (int phase = 0; phase < 2; ++phase) {
        bgfx::setViewRect(0, 0, 0, 32, 16);
        bgfx::setViewFrameBuffer(0, framebuffer.Get());
        bgfx::setViewClear(0, BGFX_CLEAR_COLOR, 0x000000ff);
        bgfx::setViewTransform(0, kIdentity.data(), kIdentity.data());
        bgfx::setTransform(kIdentity.data());
        bgfx::setVertexBuffer(0, vertices.Get());
        bgfx::setIndexBuffer(indices.Get());
        bgfx::setUniform(normal.Get(), kIdentity.data());
        bgfx::setUniform(deform.Get(), kZero.data(), 4);
        bgfx::setUniform(pivot.Get(), kZero.data(), 4);
        set("u_scene_color", {1, 1, 1, 1});
        set("u_scene_material", {0, .5f, 1, 1});
        set("u_scene_textures", {});
        set("u_scene_texture_options", {0, 1, 0, 0});
        set("u_scene_uv", {1, 1, 0, 0});
        set("u_surface_params",
            phase ? std::array<float, 4>{0, 1, 0, 1} : std::array<float, 4>{1, 0, 0, 1});
        for (std::uint8_t index = 0; index < samplers.size(); ++index)
            bgfx::setTexture(index, samplers[index].Get(), white.Get());
        bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
        bgfx::submit(0, programs[12].Get());
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
        if (pixels[kCenter + phase] < 245 || pixels[kCenter + (1 - phase)] > 10 ||
            pixels[kCenter + 2] > 10 || pixels[kCenter + 3] < 245)
            throw std::runtime_error("material_probe.tint_pixels");
        std::cout << "material phase=" << phase << " rgba=" << int(pixels[kCenter]) << ','
                  << int(pixels[kCenter + 1]) << ',' << int(pixels[kCenter + 2]) << ','
                  << int(pixels[kCenter + 3]) << '\n';
    }
    std::cout << "18 scene program pairs created; ordinary unlit surface parameter redraw passed; "
                 "PBR texture/shadow pixels and runtime adoption remain separate\n";
}
}  // namespace rhythm::validation
