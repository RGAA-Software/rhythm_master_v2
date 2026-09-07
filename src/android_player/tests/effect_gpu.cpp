#include <GLES3/gl3.h>

#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>

#include "texture_ops.h"
#include "trail_pass.h"

namespace rhythm::validation {
namespace {
render::DrawList Quad(render::TextureHandle source) {
    render::DrawList draw;
    draw.width_ = draw.height_ = 16;
    runtime::detail::AppendTextureQuad(draw, source, 0xffffffff, 0xffffffff);
    return draw;
}
std::array<std::uint8_t, 4> Pixel() {
    std::array<std::uint8_t, 4> pixel{};
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glReadPixels(8, 8, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
    if (glGetError() != GL_NO_ERROR) throw std::runtime_error("probe.effect_readback");
    return pixel;
}
}  // namespace
void VerifyEffectPixels(render::Renderer& renderer) {
    std::array<std::uint8_t, 16 * 16 * 4> pixels{};
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x) {
            const auto offset = static_cast<std::size_t>((y * 16 + x) * 4);
            pixels[offset] = static_cast<std::uint8_t>(x * 16);
            pixels[offset + 1] = static_cast<std::uint8_t>(y * 16);
            pixels[offset + 3] = 255;
        }
    auto source = renderer.CreateTexture({16, 16}, pixels);
    const std::array<std::uint8_t, 4> vector{255, 128, 0, 255};
    auto map = renderer.CreateTexture({1, 1}, vector);
    std::array<std::uint8_t, 4> baseline{};
    for (int scenario = 0; scenario < 3; ++scenario) {
        for (int frame = 0; frame < 4; ++frame) {
            renderer.BeginFrame();
            auto draw = Quad(source.Handle());
            draw.commands_[0].texture_displace_ =
                    render::TextureDisplace{map.Handle(), render::TextureDisplaceKind::kVectorRg,
                                            scenario == 0   ? 0.0f
                                            : scenario == 1 ? 0.125f
                                                            : -0.125f};
            renderer.Submit({}, draw);
            renderer.EndFrame();
        }
        const auto pixel = Pixel();
        if (scenario == 0) baseline = pixel;
        const auto expected = int(baseline[0]) + (scenario == 0 ? 0 : scenario == 1 ? 32 : -32);
        if (std::abs(int(pixel[0]) - expected) > 2 ||
            std::abs(int(pixel[1]) - int(baseline[1])) > 2)
            throw std::runtime_error("probe.displace_pixels");
    }
    const std::array<std::uint8_t, 4> white_pixel{255, 255, 255, 255}, black_pixel{};
    auto white = renderer.CreateTexture({1, 1}, white_pixel);
    auto black = renderer.CreateTexture({1, 1}, black_pixel);
    for (const int rate : {30, 60}) {
        runtime::detail::TrailPass trail;
        for (int frame = 0; frame <= rate + 4; ++frame) {
            renderer.BeginFrame();
            const auto time = std::min(frame, rate) / static_cast<double>(rate);
            const auto result = trail.Draw(frame == 0 ? white.Handle() : black.Handle(), {16, 16},
                                           time, true, {1}, renderer);
            renderer.Submit({}, Quad(result));
            renderer.EndFrame();
        }
        if (std::abs(int(Pixel()[0]) - 128) > 3)
            throw std::runtime_error("probe.float_trail_decay");
        for (int frame = 0; frame < 4; ++frame) {
            renderer.BeginFrame();
            const auto result = trail.Draw(white.Handle(), {16, 16}, 3, false, {1}, renderer);
            renderer.Submit({}, Quad(result));
            renderer.EndFrame();
        }
        if (std::abs(int(Pixel()[0]) - 128) > 3) throw std::runtime_error("probe.trail_pause");
        for (int frame = 0; frame < 4; ++frame) {
            renderer.BeginFrame();
            const auto result = trail.Draw(black.Handle(), {16, 16}, 0, true, {1}, renderer);
            renderer.Submit({}, Quad(result));
            renderer.EndFrame();
        }
        if (Pixel()[0] > 1) throw std::runtime_error("probe.trail_reverse_reset");
    }
    std::cout << "Displacement and RGBA16F trails: signed pixels, 30/60 Hz decay, pause and "
                 "reverse reset passed\n";
}
}  // namespace rhythm::validation
