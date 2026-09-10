// Native transparency experiment; no Runtime or material mode is adopted here.
#include <bgfx/bgfx.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include "bgfx_handles.h"
#include "gpu_execution_probe.h"
#include "gpu_execution_probe_shader.h"
namespace rhythm::validation {
namespace {
using render::detail::GpuHandle;
using Pixels = std::span<std::uint8_t, 32 * 16 * 4>;
constexpr auto kWrite = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A;
constexpr auto kOver = BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_INV_SRC_ALPHA);
class Programs final {
   public:
    Programs() {
        using namespace render::detail;
        GpuHandle vertex(bgfx::createShader(bgfx::copy(kTransparencyProbeVertexShader,
                                                       sizeof(kTransparencyProbeVertexShader))));
        GpuHandle color(bgfx::createShader(
                bgfx::copy(kTransparencyProbeColorShader, sizeof(kTransparencyProbeColorShader))));
        GpuHandle resolve(bgfx::createShader(bgfx::copy(kTransparencyProbeResolveShader,
                                                        sizeof(kTransparencyProbeResolveShader))));
        color_program_ = GpuHandle(bgfx::createProgram(vertex.Get(), color.Get(), false));
        resolve_program_ = GpuHandle(bgfx::createProgram(vertex.Get(), resolve.Get(), false));
        color_ = GpuHandle(bgfx::createUniform("u_transparency_color", bgfx::UniformType::Vec4));
        accumulation_ = GpuHandle(
                bgfx::createUniform("s_transparency_accumulation", bgfx::UniformType::Sampler));
        revealage_ = GpuHandle(
                bgfx::createUniform("s_transparency_revealage", bgfx::UniformType::Sampler));
        constexpr std::array<std::array<float, 12>, 3> kPlanes{
                {{-1, -1, .2f, 1, -1, .8f, 1, 1, .8f, -1, 1, .2f},
                 {-1, -1, .8f, 1, -1, .2f, 1, 1, .2f, -1, 1, .8f},
                 {-.25f, -1, .1f, .25f, -1, .1f, .25f, 1, .1f, -.25f, 1, .1f}}};
        bgfx::VertexLayout layout;
        layout.begin().add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float).end();
        for (std::size_t index = 0; index < kPlanes.size(); ++index)
            planes_[index] = GpuHandle(bgfx::createVertexBuffer(
                    bgfx::copy(kPlanes[index].data(), sizeof(kPlanes[index])), layout));
        constexpr std::array<std::uint16_t, 6> kIndices{0, 1, 2, 0, 2, 3};
        indices_ =
                GpuHandle(bgfx::createIndexBuffer(bgfx::copy(kIndices.data(), sizeof(kIndices))));
    }
    void Draw(bgfx::ViewId view, std::size_t plane, const std::array<float, 4>& color,
              std::uint64_t state) {
        bgfx::setUniform(color_.Get(), color.data());
        bgfx::setVertexBuffer(0, planes_.at(plane).Get());
        bgfx::setIndexBuffer(indices_.Get());
        bgfx::setState(state);
        bgfx::submit(view, color_program_.Get());
    }
    void Resolve(bgfx::ViewId view, bgfx::TextureHandle accumulation,
                 bgfx::TextureHandle revealage) {
        constexpr auto kSampler = BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP |
                                  BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT;
        bgfx::setTexture(0, accumulation_.Get(), accumulation, kSampler);
        bgfx::setTexture(1, revealage_.Get(), revealage, kSampler);
        bgfx::setVertexBuffer(0, planes_[0].Get());
        bgfx::setIndexBuffer(indices_.Get());
        bgfx::setState(kWrite | kOver);
        bgfx::submit(view, resolve_program_.Get());
    }

   private:
    GpuHandle<bgfx::ProgramHandle> color_program_{};
    GpuHandle<bgfx::ProgramHandle> resolve_program_{};
    GpuHandle<bgfx::UniformHandle> color_{};
    GpuHandle<bgfx::UniformHandle> accumulation_{};
    GpuHandle<bgfx::UniformHandle> revealage_{};
    std::array<GpuHandle<bgfx::VertexBufferHandle>, 3> planes_{};
    GpuHandle<bgfx::IndexBufferHandle> indices_{};
};
class Targets final {
   public:
    Targets(std::uint16_t width, std::uint16_t height, bool approximate)
        : width_(width), height_(height), approximate_(approximate) {
        output_ = Texture(bgfx::TextureFormat::RGBA8);
        depth_ = Texture(bgfx::TextureFormat::D24S8);
        output_frame_ = Frame(output_.Get());
        if (approximate_) {
            accumulation_ = Texture(bgfx::TextureFormat::RGBA16F);
            revealage_ = Texture(bgfx::TextureFormat::RGBA8);
            accumulation_frame_ = Frame(accumulation_.Get());
            revealage_frame_ = Frame(revealage_.Get());
        }
        staging_ = GpuHandle(bgfx::createTexture2D(32, 16, false, 1, bgfx::TextureFormat::RGBA8,
                                                   BGFX_TEXTURE_BLIT_DST | BGFX_TEXTURE_READ_BACK));
    }
    void Capture(Programs& programs, bool reverse, float alpha, int layers, bool opaque,
                 Pixels pixels) {
        View(0, output_frame_.Get(), BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0);
        bgfx::touch(0);
        if (opaque)
            programs.Draw(0, 2, {0, 1, 0, 1},
                          kWrite | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS);
        if (approximate_) {
            View(1, accumulation_frame_.Get(), BGFX_CLEAR_COLOR, 0);
            View(2, revealage_frame_.Get(), BGFX_CLEAR_COLOR, 0x000000ff);
        }
        for (int index = 0; index < layers; ++index) {
            const auto plane = std::size_t((reverse ? layers - 1 - index : index) % 2);
            const std::array<float, 4> color = plane ? std::array<float, 4>{0, 0, 1, alpha}
                                                     : std::array<float, 4>{1, 0, 0, alpha};
            if (approximate_) {
                programs.Draw(
                        1, plane, color,
                        kWrite | BGFX_STATE_DEPTH_TEST_LESS |
                                BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_ONE));
                programs.Draw(2, plane, color,
                              BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_LESS |
                                      BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ZERO,
                                                            BGFX_STATE_BLEND_INV_SRC_ALPHA));
            } else {
                programs.Draw(0, plane, color, kWrite | BGFX_STATE_DEPTH_TEST_LESS | kOver);
            }
        }
        if (approximate_) {
            View(3, output_frame_.Get(), 0, 0);
            programs.Resolve(3, accumulation_.Get(), revealage_.Get());
        }
        bgfx::resetView(4);
        bgfx::TextureRegion from;
        from.init(output_.Get(), std::uint16_t(width_ / 2 - 16), std::uint16_t(height_ / 2 - 8), 32,
                  16);
        bgfx::TextureRegion to;
        to.init(staging_.Get());
        bgfx::blit(4, to, from);
        bgfx::frame();
        std::fill(pixels.begin(), pixels.end(), 0);
        const auto ready = bgfx::read(to, pixels.data());
        for (int index = 0; index < 64; ++index)
            if (static_cast<std::int32_t>(bgfx::frame() - ready) >= 0) return;
        throw std::runtime_error("transparency.readback_timeout");
    }
    std::uint64_t AttachmentBytes() const {
        return std::uint64_t(width_) * height_ * (approximate_ ? 20 : 8);
    }

   private:
    GpuHandle<bgfx::TextureHandle> Texture(bgfx::TextureFormat::Enum format) {
        if (!bgfx::isTextureValid(0, false, 1, format, BGFX_TEXTURE_RT))
            throw std::runtime_error("transparency.attachment_format_unsupported");
        return GpuHandle(bgfx::createTexture2D(width_, height_, false, 1, format, BGFX_TEXTURE_RT));
    }
    GpuHandle<bgfx::FrameBufferHandle> Frame(bgfx::TextureHandle color) {
        const std::array attachments{color, depth_.Get()};
        return GpuHandle(bgfx::createFrameBuffer(2, attachments.data(), false));
    }
    void View(bgfx::ViewId view, bgfx::FrameBufferHandle framebuffer, std::uint16_t clear,
              std::uint32_t color) {
        bgfx::resetView(view);
        bgfx::setViewMode(view, bgfx::ViewMode::Sequential);
        bgfx::setViewFrameBuffer(view, framebuffer);
        bgfx::setViewRect(view, 0, 0, width_, height_);
        bgfx::setViewClear(view, clear, color, 1);
    }
    std::uint16_t width_ = 32;
    std::uint16_t height_ = 16;
    bool approximate_ = false;
    GpuHandle<bgfx::TextureHandle> output_{};
    GpuHandle<bgfx::TextureHandle> depth_{};
    GpuHandle<bgfx::TextureHandle> accumulation_{};
    GpuHandle<bgfx::TextureHandle> revealage_{};
    GpuHandle<bgfx::TextureHandle> staging_{};
    GpuHandle<bgfx::FrameBufferHandle> output_frame_{};
    GpuHandle<bgfx::FrameBufferHandle> accumulation_frame_{};
    GpuHandle<bgfx::FrameBufferHandle> revealage_frame_{};
};
void Save(const std::filesystem::path& path, Pixels pixels) {
    std::ofstream file(path, std::ios::binary);
    file << "P6\n32 16\n255\n";
    for (std::size_t index = 0; index < pixels.size(); index += 4)
        file.write(reinterpret_cast<const char*>(pixels.data() + index), 3);
    if (!file) throw std::runtime_error("transparency.evidence_write");
}
}  // namespace
void VerifyTransparencyProfile(Pixels pixels, const std::filesystem::path& directory) {
    const auto caps = bgfx::getCaps();
    constexpr auto kRequired = BGFX_CAPS_TEXTURE_BLIT | BGFX_CAPS_TEXTURE_READ_BACK;
    if (!caps || (caps->supported & kRequired) != kRequired)
        throw std::runtime_error("transparency.readback_unsupported");
    std::filesystem::create_directories(directory);
    std::ofstream report(directory / "measurements.txt");
    report << "backend=" << bgfx::getRendererName(caps->rendererType) << '\n';
    Programs programs;
    for (const auto alpha : {.1f, .5f, .95f}) {
        for (const bool approximate : {false, true}) {
            Targets target(32, 16, approximate);
            std::array<std::uint8_t, 32 * 16 * 4> first{};
            for (int order = 0; order < 2; ++order) {
                target.Capture(programs, order != 0, alpha, 2, true, pixels);
                const auto center = (8 * 32 + 16) * 4;
                if (pixels[center] || pixels[center + 1] < 254 || pixels[center + 2])
                    throw std::runtime_error("transparency.opaque_occlusion");
                double maximum_error = 0;
                double error = 0;
                for (const int x : {8, 24}) {
                    const auto pixel = (8 * 32 + x) * 4;
                    const auto red_reference = x < 16 ? alpha : alpha * (1 - alpha);
                    const auto blue_reference = x < 16 ? alpha * (1 - alpha) : alpha;
                    for (const auto channel : {0, 2}) {
                        const auto difference =
                                std::abs(double(pixels[pixel + channel]) -
                                         255 * (channel == 0 ? red_reference : blue_reference));
                        error += difference;
                        maximum_error = std::max(maximum_error, difference);
                    }
                    if (approximate) {
                        const auto expected =
                                std::lround(255 * (1 - (1 - alpha) * (1 - alpha)) * .5);
                        if (std::abs(int(pixels[pixel]) - expected) > 2 ||
                            std::abs(int(pixels[pixel + 2]) - expected) > 2)
                            throw std::runtime_error("transparency.unit_weight_formula");
                    }
                }
                int order_difference = 0;
                if (!order)
                    std::copy(pixels.begin(), pixels.end(), first.begin());
                else
                    for (std::size_t index = 0; index < pixels.size(); ++index)
                        order_difference = std::max(
                                order_difference, std::abs(int(pixels[index]) - int(first[index])));
                if (order && approximate && order_difference > 1)
                    throw std::runtime_error("transparency.order_dependence");
                if (order && !approximate && !order_difference)
                    throw std::runtime_error("transparency.baseline_did_not_change");
                report << "alpha=" << alpha << " unit_weight=" << approximate << " order=" << order
                       << " ordered_reference_channel_mae=" << error / 4
                       << " maximum_channel_error=" << maximum_error
                       << " order_difference=" << order_difference << " opaque_depth=passed\n";
                Save(directory / ("alpha-" + std::to_string(int(std::lround(alpha * 100))) +
                                  "-mode-" + std::to_string(approximate) + "-order-" +
                                  std::to_string(order) + ".ppm"),
                     pixels);
            }
        }
    }
    for (const bool approximate : {false, true}) {
        Targets target(1920, 1080, approximate);
        const auto start = std::chrono::steady_clock::now();
        for (int frame = 0; frame < 12; ++frame) {
            target.Capture(programs, frame % 2 != 0, .15f, 8, false, pixels);
            if (pixels[(8 * 32 + 16) * 4] < 20)
                throw std::runtime_error("transparency.cost_probe_empty");
        }
        report << "extent=1920x1080 layers=8 frames=12 unit_weight=" << approximate
               << " cpu_gpu_small_readback_ms="
               << std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() -
                                                            start)
                          .count()
               << " requested_attachment_bytes=" << target.AttachmentBytes() << '\n';
    }
    report.flush();
    if (!report) throw std::runtime_error("transparency.report_write");
    std::cout << "Transparency comparison completed: " << directory.string()
              << "; order stability is not exact visibility or product adoption\n";
}
}  // namespace rhythm::validation
