#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

#include "gpu_execution_probe.h"
#include "rhythm/render/renderer.h"
namespace rhythm::validation {
namespace {
render::DrawList Quad(render::TextureHandle source) {
    render::DrawList list;
    list.width_ = list.height_ = 16;
    list.vertices_ = {{0, 0, 0, 0}, {16, 0, 1, 0}, {16, 16, 1, 1}, {0, 16, 0, 1}};
    list.indices_ = {0, 1, 2, 0, 2, 3};
    list.commands_ = {{source, 0, 6, {0, 0, 16, 16}}};
    return list;
}
render::ReadbackImage Complete(render::Renderer& renderer, render::Readback ticket) {
    for (int i = 0; i < 32; ++i) {
        if (auto result = ticket.Poll()) return std::move(*result);
        renderer.BeginFrame();
        renderer.EndFrame();
    }
    throw std::runtime_error("color_pipeline.readback_timeout");
}
void Near(int value, int expected) {
    if (std::abs(value - expected) > 2)
        throw std::runtime_error("color_pipeline.pixel." + std::to_string(value) + ".expected." +
                                 std::to_string(expected));
}
}  // namespace
void VerifyColorPipeline(render::Renderer& renderer) {
    using namespace render;
    const std::array<std::uint8_t, 4> pixel{128, 64, 32, 128}, white{255, 255, 255, 255};
    auto source = renderer.CreateTexture({1, 1}, pixel);
    auto light = renderer.CreateTexture({1, 1}, white);
    auto linear = renderer.CreateTexture({16, 16}, {}, TexturePrecision::kFloat16);
    auto middle = renderer.CreateTexture({16, 16}, {}, TexturePrecision::kFloat16);
    auto filtered = renderer.CreateTexture({16, 16}, {}, TexturePrecision::kFloat16);
    auto output = renderer.CreateTexture({16, 16});
    if (renderer.Precision(linear.Handle()) != TexturePrecision::kFloat16)
        throw std::runtime_error("color_pipeline.precision");
    renderer.BeginFrame();
    auto quad = Quad(source.Handle());
    quad.commands_[0].color_pipeline_ = ColorPipeline{ColorTransfer::kSrgb, ColorTransfer::kLinear};
    renderer.Submit(linear.Handle(), quad);
    quad = Quad(linear.Handle());
    renderer.Submit(output.Handle(), quad);
    auto linear_read = renderer.RequestReadback(output.Handle());
    quad.commands_[0].color_pipeline_ = ColorPipeline{ColorTransfer::kLinear, ColorTransfer::kSrgb};
    renderer.Submit(output.Handle(), quad);
    auto roundtrip_read = renderer.RequestReadback(output.Handle());
    renderer.EndFrame();
    auto decoded = Complete(renderer, std::move(linear_read));
    Near(decoded.rgba_[0], 27);
    Near(decoded.rgba_[1], 6);
    Near(decoded.rgba_[2], 2);
    Near(decoded.rgba_[3], 128);
    auto roundtrip = Complete(renderer, std::move(roundtrip_read));
    Near(roundtrip.rgba_[0], 64);
    Near(roundtrip.rgba_[1], 32);
    Near(roundtrip.rgba_[2], 16);
    Near(roundtrip.rgba_[3], 128);
    renderer.BeginFrame();
    quad = Quad(light.Handle());
    quad.commands_[0].color_pipeline_ =
            ColorPipeline{ColorTransfer::kLinear, ColorTransfer::kLinear, ToneMapping::kNone, 2};
    renderer.Submit(linear.Handle(), quad);  // 4.0 in floating storage.
    quad = Quad(linear.Handle());
    quad.commands_[0].color_adjustment_ = ColorAdjustment{1};
    renderer.Submit(middle.Handle(), quad);  // 8.0, old color adjustment clipped this to 1.
    quad = Quad(middle.Handle());
    quad.commands_[0].texture_filter_ = TextureFilter{};
    renderer.Submit(filtered.Handle(), quad);  // Uniform input remains 8.0 across Gaussian taps.
    quad = Quad(filtered.Handle());
    quad.commands_[0].color_pipeline_ =
            ColorPipeline{ColorTransfer::kLinear, ColorTransfer::kSrgb, ToneMapping::kReinhard};
    renderer.Submit(output.Handle(), quad);
    auto hdr_read = renderer.RequestReadback(output.Handle());
    renderer.EndFrame();
    auto hdr = Complete(renderer, std::move(hdr_read));
    Near(hdr.rgba_[0], 242);
    Near(hdr.rgba_[1], 242);
    Near(hdr.rgba_[2], 242);
    Near(hdr.rgba_[3], 255);
    const std::array tone_modes{ToneMapping::kFilmic, ToneMapping::kAces, ToneMapping::kAgx};
    const std::array expected_tones{249, 253, 247};
    for (std::size_t index = 0; index < tone_modes.size(); ++index) {
        renderer.BeginFrame();
        quad = Quad(filtered.Handle());
        quad.commands_[0].color_pipeline_ =
                ColorPipeline{ColorTransfer::kLinear, ColorTransfer::kSrgb, tone_modes[index]};
        renderer.Submit(output.Handle(), quad);
        auto ticket = renderer.RequestReadback(output.Handle());
        renderer.EndFrame();
        const auto result = Complete(renderer, std::move(ticket));
        std::cout << "tone_mode=" << static_cast<int>(tone_modes[index])
                  << " rgba=" << static_cast<int>(result.rgba_[0]) << ','
                  << static_cast<int>(result.rgba_[1]) << ',' << static_cast<int>(result.rgba_[2])
                  << '\n';
        Near(result.rgba_[0], expected_tones[index]);
        Near(result.rgba_[1], expected_tones[index]);
        Near(result.rgba_[2], expected_tones[index]);
        Near(result.rgba_[3], 255);
    }
    const std::array<std::uint8_t, 4> chroma_pixel{255, 128, 32, 255};
    auto chroma = renderer.CreateTexture({1, 1}, chroma_pixel);
    renderer.BeginFrame();
    quad = Quad(chroma.Handle());
    quad.commands_[0].color_pipeline_ =
            ColorPipeline{ColorTransfer::kLinear, ColorTransfer::kLinear, ToneMapping::kNone, 3};
    renderer.Submit(middle.Handle(), quad);
    quad = Quad(middle.Handle());
    quad.commands_[0].color_pipeline_ =
            ColorPipeline{ColorTransfer::kLinear, ColorTransfer::kSrgb, ToneMapping::kAgx};
    renderer.Submit(output.Handle(), quad);
    auto chroma_ticket = renderer.RequestReadback(output.Handle());
    renderer.EndFrame();
    const auto chroma_result = Complete(renderer, std::move(chroma_ticket));
    if (!(chroma_result.rgba_[0] > chroma_result.rgba_[1] &&
          chroma_result.rgba_[1] > chroma_result.rgba_[2]))
        throw std::runtime_error("color_pipeline.agx_chroma");
    const std::array<std::uint8_t, 16> ramp_pixels{255, 64, 16,  255, 32,  255, 64,  255,
                                                   16,  64, 255, 255, 255, 255, 255, 255};
    auto ramp = renderer.CreateTexture({4, 1}, ramp_pixels);
    renderer.BeginFrame();
    quad = Quad(ramp.Handle());
    quad.commands_[0].color_pipeline_ =
            ColorPipeline{ColorTransfer::kLinear, ColorTransfer::kLinear, ToneMapping::kNone, 3};
    renderer.Submit(middle.Handle(), quad);
    renderer.EndFrame();
    const std::array all_tone_modes{ToneMapping::kReinhard, ToneMapping::kFilmic,
                                    ToneMapping::kAces, ToneMapping::kAgx};
    for (const auto mode : all_tone_modes) {
        renderer.BeginFrame();
        quad = Quad(middle.Handle());
        quad.commands_[0].color_pipeline_ =
                ColorPipeline{ColorTransfer::kLinear, ColorTransfer::kSrgb, mode};
        renderer.Submit(output.Handle(), quad);
        auto ticket = renderer.RequestReadback(output.Handle());
        renderer.EndFrame();
        const auto result = Complete(renderer, std::move(ticket));
        const auto channel = [&](int x, int component) {
            return result.rgba_[(8 * 16 + x) * 4 + component];
        };
        std::cout << "hdr_ramp mode=" << static_cast<int>(mode);
        for (const int x : {2, 6, 10, 14})
            std::cout << " [" << int(channel(x, 0)) << ',' << int(channel(x, 1)) << ','
                      << int(channel(x, 2)) << ']';
        std::cout << '\n';
        if (!(channel(2, 0) > channel(2, 1) && channel(2, 1) > channel(2, 2) &&
              channel(6, 1) > channel(6, 2) && channel(6, 2) > channel(6, 0) &&
              channel(10, 2) > channel(10, 1) && channel(10, 1) > channel(10, 0) &&
              std::abs(int(channel(14, 0)) - int(channel(14, 1))) <= 2 &&
              std::abs(int(channel(14, 1)) - int(channel(14, 2))) <= 2))
            throw std::runtime_error("color_pipeline.hdr_chromatic_ramp");
    }
    renderer.BeginFrame();
    quad = Quad(source.Handle());
    quad.commands_[0].blend_ = BlendMode::kAdd;
    quad.commands_.push_back(quad.commands_[0]);
    renderer.Submit(linear.Handle(), quad);
    quad = Quad(linear.Handle());
    renderer.Submit(output.Handle(), quad);
    auto added_read = renderer.RequestReadback(output.Handle());
    renderer.EndFrame();
    auto added = Complete(renderer, std::move(added_read));
    Near(added.rgba_[0], 128);
    Near(added.rgba_[3], 192);  // HDR addition retains bounded alpha coverage, not 1.004.
    std::cout << "Color pipeline: linear transfer/alpha roundtrip, neutral and chromatic 8x HDR "
                 "through Godot Reinhard/Filmic/ACES/AgX passed\n";
}
}  // namespace rhythm::validation
