#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

#include "gpu_execution_probe.h"
#include "rhythm/render/renderer.h"

namespace rhythm::validation {
namespace {
constexpr std::uint16_t kSize = 32;
render::DrawList Quad(render::TextureHandle texture) {
    render::DrawList result;
    result.width_ = result.height_ = kSize;
    result.vertices_ = {{0, 0, 0, 0}, {kSize, 0, 1, 0}, {kSize, kSize, 1, 1}, {0, kSize, 0, 1}};
    result.indices_ = {0, 1, 2, 0, 2, 3};
    result.commands_ = {{texture, 0, 6, {0, 0, kSize, kSize}}};
    return result;
}
render::ReadbackImage Complete(render::Renderer& renderer, render::Readback ticket) {
    for (int frame = 0; frame < 32; ++frame) {
        if (auto result = ticket.Poll()) return std::move(*result);
        renderer.BeginFrame();
        renderer.EndFrame();
    }
    throw std::runtime_error("fxaa.readback_timeout");
}
void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}  // namespace

void VerifyFxaa(render::Renderer& renderer) {
    using namespace render;
    auto output = renderer.CreateTexture({kSize, kSize});
    for (int scenario = 0; scenario < 3; ++scenario) {
        std::array<std::uint8_t, kSize * kSize * 4> pixels{};
        for (int y = 0; y < kSize; ++y) {
            for (int x = 0; x < kSize; ++x) {
                const auto index = (y * kSize + x) * 4;
                const bool inside = x > y * 0.63 + 4;
                pixels[index] = inside && scenario != 2 ? 255 : 0;
                pixels[index + 1] = inside && scenario == 0 ? 255 : 0;
                pixels[index + 2] = inside && scenario == 0 ? 255 : 0;
                pixels[index + 3] = inside || scenario == 0 ? 255 : 0;
            }
        }
        auto source = renderer.CreateTexture({kSize, kSize}, pixels);
        auto draw = Quad(source.Handle());
        renderer.BeginFrame();
        renderer.Submit(output.Handle(), draw);
        auto original_ticket = renderer.RequestReadback(output.Handle());
        draw.commands_[0].texture_fxaa_ = TextureFxaa{};
        renderer.Submit(output.Handle(), draw);
        auto filtered_ticket = renderer.RequestReadback(output.Handle());
        draw.commands_[0].texture_fxaa_->strength_ = 0;
        renderer.Submit(output.Handle(), draw);
        auto bypass_ticket = renderer.RequestReadback(output.Handle());
        renderer.EndFrame();
        const auto original = Complete(renderer, std::move(original_ticket));
        const auto filtered = Complete(renderer, std::move(filtered_ticket));
        const auto bypass = Complete(renderer, std::move(bypass_ticket));
        Check(original.rgba_ == bypass.rgba_, "fxaa.zero_strength_identity");
        int fractional = 0;
        std::uint64_t original_energy = 0, filtered_energy = 0;
        for (std::size_t i = 0; i < pixels.size(); i += 4) {
            const auto channel = scenario == 0 ? 0 : 3;
            const auto value = filtered.rgba_[i + channel];
            if (value > 2 && value < 253) ++fractional;
            original_energy += original.rgba_[i + channel];
            filtered_energy += value;
            for (std::size_t c = 0; c < 3; ++c)
                Check(filtered.rgba_[i + c] <= filtered.rgba_[i + 3] + 1,
                      "fxaa.premultiplied_edge");
            if (scenario == 2)
                Check(filtered.rgba_[i] == 0 && filtered.rgba_[i + 1] == 0 &&
                              filtered.rgba_[i + 2] == 0,
                      "fxaa.black_alpha_edge_color");
        }
        Check(fractional >= 16, "fxaa.diagonal_edge_not_smoothed");
        Check(std::abs(double(original_energy) - double(filtered_energy)) < original_energy * 0.03,
              "fxaa.edge_energy");
        for (std::size_t i : {std::size_t(0), pixels.size() - 4})
            for (std::size_t c = 0; c < 4; ++c)
                Check(std::abs(int(filtered.rgba_[i + c]) - int(original.rgba_[i + c])) <= 1,
                      "fxaa.uniform_region_changed");
        std::cout << "FXAA scenario=" << scenario << " fractional_edge_pixels=" << fractional
                  << '\n';
    }
    // Degenerate source extent, fractional coverage, and extreme legal controls.
    const std::array<std::uint8_t, 4> uniform{128, 64, 32, 128};
    auto source = renderer.CreateTexture({1, 1}, uniform);
    auto draw = Quad(source.Handle());
    renderer.BeginFrame();
    renderer.Submit(output.Handle(), draw);
    auto uniform_ticket = renderer.RequestReadback(output.Handle());
    draw.commands_[0].texture_fxaa_ = TextureFxaa{16, 1, 0.001f, 1};
    renderer.Submit(output.Handle(), draw);
    auto filtered_ticket = renderer.RequestReadback(output.Handle());
    renderer.EndFrame();
    Check(Complete(renderer, std::move(uniform_ticket)).rgba_ ==
                  Complete(renderer, std::move(filtered_ticket)).rgba_,
          "fxaa.uniform_texture_identity");
    renderer.BeginFrame();
    for (const auto invalid : {TextureFxaa{0}, TextureFxaa{17}, TextureFxaa{8, 0},
                               TextureFxaa{8, 0.125f, 0}, TextureFxaa{8, 0.125f, 0.01f, -1},
                               TextureFxaa{std::numeric_limits<float>::quiet_NaN()}}) {
        draw.commands_[0].texture_fxaa_ = invalid;
        bool rejected = false;
        try {
            renderer.Submit(output.Handle(), draw);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        Check(rejected, "fxaa.invalid_parameter_accepted");
    }
    renderer.EndFrame();
    std::cout << "FXAA: opaque/color/black-alpha edges, premultiplication, zero strength, "
                 "uniform source and bounded parameters passed\n";
}
}  // namespace rhythm::validation
