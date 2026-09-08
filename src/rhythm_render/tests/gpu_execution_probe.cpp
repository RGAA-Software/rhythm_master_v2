// Test-only native adapter. All bgfx resources stay on the active device thread.
#include "gpu_execution_probe.h"

#include <bgfx/bgfx.h>

#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>

#include "bgfx_handles.h"
#include "gpu_execution_probe_shader.h"

namespace rhythm::validation {
void VerifyGpuExecution(std::span<std::uint8_t, 32 * 16 * 4> pixels) {
    using namespace render::detail;
    const auto caps = bgfx::getCaps();
    if (!caps) throw std::runtime_error("probe.no_device");
    const bool instancing = (caps->supported & BGFX_CAPS_INSTANCING) != 0;
    const bool compute = (caps->supported & BGFX_CAPS_COMPUTE) != 0;
    std::cout << "backend=" << bgfx::getRendererName(caps->rendererType)
              << " vendor=" << caps->vendorId << " device=" << caps->deviceId
              << " instancing=" << instancing << " compute=" << compute << '\n';
    if (!instancing) throw std::runtime_error("probe.instancing_unsupported");
    const auto required = BGFX_CAPS_TEXTURE_BLIT | BGFX_CAPS_TEXTURE_READ_BACK;
    if ((caps->supported & required) != required)
        throw std::runtime_error("probe.readback_unsupported");
    GpuHandle vertex_shader(
            bgfx::createShader(bgfx::copy(kProbeVertexShader, sizeof(kProbeVertexShader))));
    GpuHandle fragment_shader(
            bgfx::createShader(bgfx::copy(kProbeFragmentShader, sizeof(kProbeFragmentShader))));
    GpuHandle program(bgfx::createProgram(vertex_shader.Get(), fragment_shader.Get(), false));
    GpuHandle<bgfx::ShaderHandle> compute_shader;
    GpuHandle<bgfx::ProgramHandle> compute_program;
    if (compute) {
        compute_shader = GpuHandle(
                bgfx::createShader(bgfx::copy(kProbeComputeShader, sizeof(kProbeComputeShader))));
        compute_program = GpuHandle(bgfx::createProgram(compute_shader.Get(), false));
    }
    GpuHandle phase(bgfx::createUniform("u_probe_phase", bgfx::UniformType::Vec4));
    const std::array<float, 8> vertices{-0.4f, -0.4f, 0.4f, -0.4f, 0.4f, 0.4f, -0.4f, 0.4f};
    const std::array<std::uint16_t, 6> indices{0, 1, 2, 0, 2, 3};
    const std::array<float, 8> initial{-0.5f, 0, 1, 0, 0.5f, 0, 0, 1};
    bgfx::VertexLayout vertex_layout;
    vertex_layout.begin().add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float).end();
    bgfx::VertexLayout instance_layout;
    instance_layout.begin().add(bgfx::Attrib::TexCoord0, 4, bgfx::AttribType::Float).end();
    GpuHandle vertex_buffer(
            bgfx::createVertexBuffer(bgfx::copy(vertices.data(), sizeof(vertices)), vertex_layout));
    GpuHandle index_buffer(bgfx::createIndexBuffer(bgfx::copy(indices.data(), sizeof(indices))));
    GpuHandle cpu_instances(bgfx::createDynamicVertexBuffer(
            bgfx::copy(initial.data(), sizeof(initial)), instance_layout));
    GpuHandle<bgfx::DynamicVertexBufferHandle> gpu_instances;
    if (compute)
        gpu_instances = GpuHandle(bgfx::createDynamicVertexBuffer(2, instance_layout,
                                                                  BGFX_BUFFER_COMPUTE_READ_WRITE));
    GpuHandle color(
            bgfx::createTexture2D(32, 16, false, 1, bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_RT));
    const auto attachment = color.Get();
    GpuHandle framebuffer(bgfx::createFrameBuffer(1, &attachment, false));
    GpuHandle readback(bgfx::createTexture2D(32, 16, false, 1, bgfx::TextureFormat::RGBA8,
                                             BGFX_TEXTURE_BLIT_DST | BGFX_TEXTURE_READ_BACK));
    // The same target and instance buffer are reused. The second compute write
    // must replace the first frame's colors without any CPU instance upload.
    for (int scenario = 0; scenario < (compute ? 3 : 1); ++scenario) {
        if (scenario > 0) {
            const std::array<float, 4> value{static_cast<float>(scenario % 2), 0, 0, 0};
            bgfx::setUniform(phase.Get(), value.data());
            bgfx::setBuffer(0, gpu_instances.Get(), bgfx::Access::ReadWrite);
            bgfx::dispatch(0, compute_program.Get(), 1);
        }
        bgfx::setViewRect(1, 0, 0, 32, 16);
        bgfx::setViewFrameBuffer(1, framebuffer.Get());
        bgfx::setViewClear(1, BGFX_CLEAR_COLOR, 0x000000ff);
        bgfx::setVertexBuffer(0, vertex_buffer.Get());
        bgfx::setIndexBuffer(index_buffer.Get());
        bgfx::setInstanceDataBuffer(scenario == 0 ? cpu_instances.Get() : gpu_instances.Get(), 0,
                                    2);
        bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
        bgfx::submit(1, program.Get());
        bgfx::TextureRegion source;
        source.init(color.Get());
        bgfx::TextureRegion staging;
        staging.init(readback.Get());
        bgfx::blit(2, staging, source);
        bgfx::frame();
        std::fill(pixels.begin(), pixels.end(), 0);
        const auto ready = bgfx::read(staging, pixels.data());
        bool completed = false;
        for (int wait = 0; wait < 64; ++wait) {
            if (static_cast<std::int32_t>(bgfx::frame() - ready) >= 0) {
                completed = true;
                break;
            }
        }
        if (!completed) throw std::runtime_error("probe.readback_timeout");
        for (std::size_t index = 0; index < 2; ++index) {
            const auto offset = (8 * 32 + 8 + index * 16) * 4;
            const bool green = (index + scenario) % 2 == 1;
            std::cout << "scenario=" << scenario << " sample=" << index
                      << " rgba=" << int(pixels[offset]) << ',' << int(pixels[offset + 1]) << ','
                      << int(pixels[offset + 2]) << ',' << int(pixels[offset + 3]) << '\n';
            if (pixels[offset + (green ? 1 : 0)] < 240 || pixels[offset + (green ? 0 : 1)] > 10 ||
                pixels[offset + 2] > 10 || pixels[offset + 3] < 240)
                throw std::runtime_error("probe.instance_compute_pixels");
        }
        std::cout << "scenario=" << scenario << " instances=2 draws=1 pixels=passed\n";
    }
    std::cout << "instancing_pixels=passed compute_write_draw_rewrite="
              << (compute ? "passed" : "unsupported") << '\n';
}
}  // namespace rhythm::validation
