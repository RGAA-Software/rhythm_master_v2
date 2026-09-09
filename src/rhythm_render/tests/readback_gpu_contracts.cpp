#include <array>
#include <iostream>
#include <stdexcept>
#include <utility>

#include "rhythm/render/renderer.h"

namespace rhythm::validation {
namespace {
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
template <typename Function>
void Reject(Function function) {
    bool rejected = false;
    try {
        function();
    } catch (const std::exception&) {
        rejected = true;
    }
    Require(rejected, "invalid readback accepted");
}
render::ReadbackImage Complete(render::Renderer& renderer, render::Readback& ticket) {
    for (int frame = 0; frame < 16; ++frame) {
        if (auto image = ticket.Poll()) return std::move(*image);
        renderer.BeginFrame();
        renderer.EndFrame();
    }
    throw std::runtime_error("readback did not complete");
}
}  // namespace
void VerifyReadbackPixels(render::Renderer& renderer) {
    using namespace render;
    const auto baseline = renderer.Stats().texture_bytes_;
    Require(renderer.SupportsReadback(), "readback capability required by this contract");
    {
        std::vector<std::uint8_t> pixels(16 * 16 * 4);
        for (std::size_t index = 0; index < pixels.size(); index += 4) {
            pixels[index] = index < pixels.size() / 2 ? 200 : 20;
            pixels[index + 1] = 40;
            pixels[index + 2] = index < pixels.size() / 2 ? 30 : 180;
            pixels[index + 3] = 128;
        }
        auto source = renderer.CreateTexture({16, 16}, pixels);
        auto target = renderer.CreateTexture({16, 16});
        auto floating = renderer.CreateTexture({16, 16}, {}, TexturePrecision::kFloat16);
        DrawList quad;
        quad.width_ = quad.height_ = 16;
        quad.vertices_ = {{0, 0, 0, 0}, {16, 0, 1, 0}, {16, 16, 1, 1}, {0, 16, 0, 1}};
        quad.indices_ = {0, 1, 2, 0, 2, 3};
        quad.commands_ = {{source.Handle(), 0, 6, {0, 0, 16, 16}}};
        DrawList empty;
        empty.width_ = empty.height_ = 16;
        Reject([&] { renderer.RequestReadback(target.Handle()); });
        renderer.BeginFrame();
        Reject([&] { renderer.RequestReadback(floating.Handle()); });
        Reject([&] { renderer.RequestReadback(source.Handle()); });
        Reject([&] { renderer.RequestReadback({}); });
        renderer.Submit(target.Handle(), quad);
        auto first = renderer.RequestReadback(target.Handle());
        Require(!first.Poll(), "readback must be asynchronous");
        auto moved = std::move(first);
        Reject([&] { first.Poll(); });
        // Later writes must not replace the captured frame's pixels.
        renderer.Submit(target.Handle(), empty, 0x00ff00ff);
        auto green = renderer.RequestReadback(target.Handle());
        renderer.Submit(target.Handle(), empty, 0x0000ffff);
        auto blue = renderer.RequestReadback(target.Handle());
        Reject([&] { renderer.RequestReadback(target.Handle()); });
        Require(renderer.Stats().texture_bytes_ == baseline + 7 * 16 * 16 * 4,
                "readback staging participates in the shared texture budget");
        renderer.EndFrame();
        const auto captured = Complete(renderer, moved);
        Require(captured.extent_ == Extent{16, 16} && captured.rgba_.size() == pixels.size(),
                "readback dimensions");
        for (const int y : {3, 12})
            for (int channel = 0; channel < 4; ++channel) {
                const auto index = static_cast<std::size_t>((y * 16 + 8) * 4 + channel);
                const int expected = channel == 3 ? 128 : (pixels[index] * 128 + 127) / 255;
                Require(std::abs(static_cast<int>(captured.rgba_[index]) - expected) <= 1,
                        "readback preserves frame order, top-left orientation and premultiplied "
                        "RGBA");
            }
        for (const auto& color : {std::array<int, 3>{0, 255, 0}, std::array<int, 3>{0, 0, 255}}) {
            const auto image = Complete(renderer, color[1] ? green : blue);
            for (int channel = 0; channel < 3; ++channel)
                Require(image.rgba_[channel] == color[channel], "queued readback frame identity");
        }
        Reject([&] { moved.Poll(); });
        // Cancellation both before issuing read and after issuing it must retain
        // pending memory, reject early slot reuse, then reclaim every reservation.
        renderer.BeginFrame();
        auto canceled = renderer.RequestReadback(target.Handle());
        canceled = {};
        renderer.EndFrame();
        renderer.BeginFrame();
        canceled = renderer.RequestReadback(target.Handle());
        renderer.EndFrame();
        canceled = {};
        for (int frame = 0; frame < 8; ++frame) {
            renderer.BeginFrame();
            renderer.EndFrame();
        }
        Require(renderer.Stats().texture_bytes_ == baseline + 4 * 16 * 16 * 4,
                "cancellation reclaims staging after completion");
        const auto handle = target.Handle();
        auto retained = renderer.RetainTexture(handle);
        target = {};
        Require(renderer.IsValid(handle), "retained presentation survives original owner release");
        renderer.BeginFrame();
        auto frozen = renderer.RequestReadback(retained.Handle());
        renderer.EndFrame();
        const auto frozen_pixels = Complete(renderer, frozen);
        for (std::size_t offset = 0; offset < frozen_pixels.rgba_.size(); offset += 4)
            Require(frozen_pixels.rgba_[offset] == 0 && frozen_pixels.rgba_[offset + 1] == 0 &&
                            frozen_pixels.rgba_[offset + 2] == 255 &&
                            frozen_pixels.rgba_[offset + 3] == 255,
                    "retained target preserves pixels after releasing its producing owner");
        retained = {};
        Require(!renderer.IsValid(handle), "last presentation lease releases native texture");
    }
    Require(renderer.Stats().texture_bytes_ == baseline, "readback contract releases resources");
    std::cout << "readback: ordered RGBA frames, three-slot budget, move-only delivery and "
                 "cancellation passed\n";
}
}  // namespace rhythm::validation
