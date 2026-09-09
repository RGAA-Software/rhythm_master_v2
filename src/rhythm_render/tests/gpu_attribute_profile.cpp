// Native P6.2 experiment. No new public buffer layout or Runtime operation.
#include <bgfx/bgfx.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "bgfx_handles.h"
#include "gpu_execution_probe.h"
#include "gpu_execution_probe_shader.h"

namespace rhythm::validation {
namespace {
using render::detail::GpuHandle;
using Record = std::array<float, 16>;
static_assert(sizeof(Record) == 64);
constexpr std::array<float, 16> kTransform{.5f, 0, 0,   0, 0,     .5f,   0,     0,
                                           0,   0, .5f, 0, .125f, .125f, .125f, 1};
Record Initial(std::uint32_t index) {
    return {float(index % 128 + 1) / 256,
            .25f,
            .125f,
            .5f,
            .1f,
            float(index / 128 % 128 + 1) / 256,
            float(index / 16384 + 1) / 32,
            .9f,
            .2f,
            .4f,
            .6f,
            .8f,
            .25f,
            .5f,
            .625f,
            .75f};
}
Record Mapped(Record value, float size, const std::array<float, 4>& color) {
    for (std::size_t i = 0; i < 3; ++i) value[i] = value[i] * .5f + .125f;
    for (std::size_t i = 0; i < 4; ++i) value[8 + i] *= color[i];
    value[12] *= size;
    return value;
}
GpuHandle<bgfx::DynamicVertexBufferHandle> Buffer(std::uint32_t count, bool source) {
    if (!count || count > 262144) throw std::invalid_argument("attribute_probe.capacity");
    std::vector<Record> values(count);
    for (std::uint32_t index = 0; index < count; ++index) {
        if (source)
            values[index] = Initial(index);
        else
            values[index].fill(.9f);
    }
    bgfx::VertexLayout layout;
    layout.begin()
            .add(bgfx::Attrib::TexCoord0, 4, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord1, 4, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord2, 4, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord3, 4, bgfx::AttribType::Float)
            .end();
    return GpuHandle(bgfx::createDynamicVertexBuffer(
            bgfx::copy(values.data(), static_cast<std::uint32_t>(count * sizeof(Record))), layout,
            BGFX_BUFFER_COMPUTE_READ_WRITE));
}
class AttributeProbe final {
   public:
    AttributeProbe() {
        using namespace render::detail;
        const auto caps = bgfx::getCaps();
        constexpr auto kRequired = BGFX_CAPS_COMPUTE | BGFX_CAPS_INSTANCING |
                                   BGFX_CAPS_TEXTURE_BLIT | BGFX_CAPS_TEXTURE_READ_BACK;
        if (!caps || (caps->supported & kRequired) != kRequired)
            throw std::runtime_error("attribute_probe.unsupported");
        std::cout << "attribute_backend=" << bgfx::getRendererName(caps->rendererType) << '\n';
        GpuHandle vertex(bgfx::createShader(
                bgfx::copy(kAttributeProbeVertexShader, sizeof(kAttributeProbeVertexShader))));
        GpuHandle fragment(
                bgfx::createShader(bgfx::copy(kProbeFragmentShader, sizeof(kProbeFragmentShader))));
        GpuHandle compute(bgfx::createShader(
                bgfx::copy(kAttributeProbeComputeShader, sizeof(kAttributeProbeComputeShader))));
        draw_ = GpuHandle(bgfx::createProgram(vertex.Get(), fragment.Get(), false));
        map_ = GpuHandle(bgfx::createProgram(compute.Get(), false));
        transform_ =
                GpuHandle(bgfx::createUniform("u_attribute_transform", bgfx::UniformType::Mat4));
        info_ = GpuHandle(bgfx::createUniform("u_attribute_info", bgfx::UniformType::Vec4));
        color_ = GpuHandle(bgfx::createUniform("u_attribute_color", bgfx::UniformType::Vec4));
        probe_ = GpuHandle(bgfx::createUniform("u_attribute_probe", bgfx::UniformType::Vec4));
        constexpr std::array<float, 8> kVertices{-1, -1, 1, -1, 1, 1, -1, 1};
        constexpr std::array<std::uint16_t, 6> kIndices{0, 1, 2, 0, 2, 3};
        bgfx::VertexLayout layout;
        layout.begin().add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float).end();
        vertices_ = GpuHandle(
                bgfx::createVertexBuffer(bgfx::copy(kVertices.data(), sizeof(kVertices)), layout));
        indices_ =
                GpuHandle(bgfx::createIndexBuffer(bgfx::copy(kIndices.data(), sizeof(kIndices))));
        target_ = GpuHandle(bgfx::createTexture2D(128, 4, false, 1, bgfx::TextureFormat::RGBA8,
                                                  BGFX_TEXTURE_RT));
        const auto attachment = target_.Get();
        framebuffer_ = GpuHandle(bgfx::createFrameBuffer(1, &attachment, false));
        staging_ = GpuHandle(bgfx::createTexture2D(128, 4, false, 1, bgfx::TextureFormat::RGBA8,
                                                   BGFX_TEXTURE_BLIT_DST | BGFX_TEXTURE_READ_BACK));
    }
    void Map(bgfx::ViewId view, bgfx::DynamicVertexBufferHandle source,
             bgfx::DynamicVertexBufferHandle destination, std::uint32_t count, float size,
             const std::array<float, 4>& color) {
        if (source.idx == destination.idx) throw std::invalid_argument("attribute_probe.alias");
        const std::array<float, 4> info{float(count), size, 0, 0};
        bgfx::resetView(view);
        bgfx::setViewMode(view, bgfx::ViewMode::Sequential);
        bgfx::setViewName(view, "Attribute profile copy/map");
        bgfx::setUniform(transform_.Get(), kTransform.data());
        bgfx::setUniform(info_.Get(), info.data());
        bgfx::setUniform(color_.Get(), color.data());
        bgfx::setBuffer(0, source, bgfx::Access::Read);
        bgfx::setBuffer(1, destination, bgfx::Access::Write);
        bgfx::dispatch(view, map_.Get(), (count + 63) / 64);
    }
    void Read(bgfx::DynamicVertexBufferHandle buffer, std::uint32_t field,
              std::span<std::uint8_t, 32 * 16 * 4> pixels, std::uint32_t offset = 0) {
        bgfx::resetView(2);
        bgfx::setViewMode(2, bgfx::ViewMode::Sequential);
        bgfx::setViewFrameBuffer(2, framebuffer_.Get());
        bgfx::setViewRect(2, 0, 0, 128, 4);
        bgfx::setViewClear(2, BGFX_CLEAR_COLOR, 0);
        for (std::uint32_t index = 0; index < 128; ++index) {
            const std::array<float, 4> probe{float(index) + .5f, float(field), 0, 0};
            bgfx::setUniform(probe_.Get(), probe.data());
            bgfx::setVertexBuffer(0, vertices_.Get());
            bgfx::setIndexBuffer(indices_.Get());
            bgfx::setInstanceDataBuffer(buffer, offset + index, 1);
            bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
            bgfx::submit(2, draw_.Get());
        }
        bgfx::TextureRegion source;
        source.init(target_.Get());
        bgfx::TextureRegion staging;
        staging.init(staging_.Get());
        bgfx::blit(3, staging, source);
        bgfx::frame();
        std::fill(pixels.begin(), pixels.end(), 0);
        const auto ready = bgfx::read(staging, pixels.data());
        for (int attempt = 0; attempt < 64; ++attempt)
            if (static_cast<std::int32_t>(bgfx::frame() - ready) >= 0) return;
        throw std::runtime_error("attribute_probe.readback_timeout");
    }

   private:
    GpuHandle<bgfx::ProgramHandle> draw_{};
    GpuHandle<bgfx::ProgramHandle> map_{};
    GpuHandle<bgfx::UniformHandle> transform_{};
    GpuHandle<bgfx::UniformHandle> info_{};
    GpuHandle<bgfx::UniformHandle> color_{};
    GpuHandle<bgfx::UniformHandle> probe_{};
    GpuHandle<bgfx::VertexBufferHandle> vertices_{};
    GpuHandle<bgfx::IndexBufferHandle> indices_{};
    GpuHandle<bgfx::TextureHandle> target_{};
    GpuHandle<bgfx::TextureHandle> staging_{};
    GpuHandle<bgfx::FrameBufferHandle> framebuffer_{};
};
void Verify(AttributeProbe& probe, bgfx::DynamicVertexBufferHandle buffer,
            const std::array<Record, 128>& expected, std::span<std::uint8_t, 32 * 16 * 4> pixels,
            std::uint32_t offset = 0) {
    for (std::uint32_t field = 0; field < 4; ++field) {
        probe.Read(buffer, field, pixels, offset);
        for (std::size_t index = 0; index < expected.size(); ++index)
            for (std::size_t channel = 0; channel < 4; ++channel) {
                const auto actual = int(pixels[(2 * 128 + index) * 4 + channel]);
                const auto reference = std::lround(expected[index][field * 4 + channel] * 255);
                if (std::abs(actual - reference) > 2)
                    throw std::runtime_error(
                            "attribute_probe.field_mismatch:" + std::to_string(offset + index) +
                            ":" + std::to_string(field * 4 + channel) + ":" +
                            std::to_string(actual) + ":" + std::to_string(reference));
            }
    }
}
}  // namespace
void VerifyGpuAttributeProfile(std::span<std::uint8_t, 32 * 16 * 4> pixels) {
    AttributeProbe probe;
    const std::array<float, 4> color{.5f, 1, .25f, 1};
    for (const std::uint32_t capacity : {128u, 65536u, 262144u}) {
        const auto count = capacity == 128 ? 65 : capacity;
        auto source = Buffer(capacity, true);
        auto first = Buffer(capacity, false);
        auto second = Buffer(capacity, false);
        std::array<Record, 128> original{};
        std::array<Record, 128> expected_first{};
        std::array<Record, 128> expected_second{};
        for (std::uint32_t index = 0; index < 128; ++index) original[index] = Initial(index);
        Verify(probe, source.Get(), original, pixels);
        for (const float size : {.75f, .5f}) {
            const auto started = std::chrono::steady_clock::now();
            for (int frame = 0; frame < 8; ++frame) {
                probe.Map(0, source.Get(), first.Get(), count, size, color);
                probe.Map(1, first.Get(), second.Get(), count, size, color);
                bgfx::frame();
            }
            probe.Read(second.Get(), 0, pixels);
            const auto elapsed = std::chrono::duration<double, std::milli>(
                                         std::chrono::steady_clock::now() - started)
                                         .count();
            const std::vector<std::uint32_t> offsets =
                    capacity == 128 ? std::vector<std::uint32_t>{0}
                                    : std::vector<std::uint32_t>{0, capacity / 2, capacity - 128};
            for (const auto offset : offsets) {
                for (std::uint32_t index = 0; index < 128; ++index) {
                    original[index] = Initial(offset + index);
                    expected_first[index].fill(.9f);
                    expected_second[index].fill(.9f);
                    if (offset + index >= count) continue;
                    expected_first[index] = Mapped(original[index], size, color);
                    expected_second[index] = Mapped(expected_first[index], size, color);
                }
                Verify(probe, source.Get(), original, pixels, offset);
                Verify(probe, first.Get(), expected_first, pixels, offset);
                Verify(probe, second.Get(), expected_second, pixels, offset);
            }
            std::cout << "attribute_count=" << count << " capacity=" << capacity << " size=" << size
                      << " chained_frames=8 requested_buffer_bytes="
                      << std::uint64_t(capacity) * 64 * 3
                      << " submit_plus_final_readback_ms=" << elapsed
                      << " sampled_records=" << offsets.size() * 128
                      << " all_16_fields=passed input_unchanged=passed tail_guard="
                      << (count < capacity ? "passed" : "not_applicable") << '\n';
        }
    }
}
}  // namespace rhythm::validation
