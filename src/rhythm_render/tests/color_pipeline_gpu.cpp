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
    std::cout << "Color pipeline: linear transfer/alpha roundtrip, 8x HDR through "
                 "color/filter/tone map passed\n";
}
}  // namespace rhythm::validation
