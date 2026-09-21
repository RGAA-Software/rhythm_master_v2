#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <vector>

#include "gpu_execution_probe.h"
#include "rhythm/render/renderer.h"

// Antialiasing path evaluation (W1.5c): measures how the shipped spatial FXAA
// node treats moving thin lines and particle-scale bright spots, quantifying
// the documented tradeoff (edge softening versus no temporal stabilization).
// The numbers printed here feed docs/antialiasing.md and the audit record.

namespace rhythm::validation {
namespace {
constexpr std::uint16_t kSize = 64;
constexpr int kFrames = 8;

render::ReadbackImage Complete(render::Renderer& renderer, render::Readback ticket) {
    for (int frame = 0; frame < 32; ++frame) {
        if (auto result = ticket.Poll()) return std::move(*result);
        renderer.BeginFrame();
        renderer.EndFrame();
    }
    throw std::runtime_error("aa_motion.readback_timeout");
}
void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct Frame {
    int peak = 0;
    int fractional = 0;
    std::uint64_t energy = 0;
    std::vector<std::uint8_t> luma;
};
struct Metrics {
    double mean_peak = 0;
    double mean_fractional = 0;
    double energy_cv = 0;
    double flicker = 0;  // Mean per-pixel absolute difference between frames.
};

// Black background plus one white quad; fractional coordinates exercise
// subpixel rasterizer coverage.
render::DrawList Scene(render::TextureHandle black, render::TextureHandle white, float x0,
                       float y0, float x1, float y1, float x2, float y2, float x3, float y3) {
    render::DrawList result;
    result.width_ = result.height_ = kSize;
    result.vertices_ = {{0, 0, 0, 0},
                        {kSize, 0, 1, 0},
                        {kSize, kSize, 1, 1},
                        {0, kSize, 0, 1},
                        {x0, y0, 0, 0},
                        {x1, y1, 1, 0},
                        {x2, y2, 1, 1},
                        {x3, y3, 0, 1}};
    result.indices_ = {0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7};
    result.commands_ = {{black, 0, 6, {0, 0, kSize, kSize}}, {white, 6, 6, {0, 0, kSize, kSize}}};
    return result;
}
render::DrawList FxaaPass(render::TextureHandle source) {
    render::DrawList result;
    result.width_ = result.height_ = kSize;
    result.vertices_ = {{0, 0, 0, 0}, {kSize, 0, 1, 0}, {kSize, kSize, 1, 1}, {0, kSize, 0, 1}};
    result.indices_ = {0, 1, 2, 0, 2, 3};
    result.commands_ = {{source, 0, 6, {0, 0, kSize, kSize}}};
    result.commands_[0].texture_fxaa_ = render::TextureFxaa{};
    return result;
}

template <typename MakeScene>
Metrics RunSeries(render::Renderer& renderer, MakeScene make_scene, bool fxaa) {
    auto scene = renderer.CreateTexture({kSize, kSize});
    auto filtered = renderer.CreateTexture({kSize, kSize});
    std::vector<Frame> frames;
    for (int frame_index = 0; frame_index < kFrames; ++frame_index) {
        renderer.BeginFrame();
        renderer.Submit(scene.Handle(), make_scene(frame_index));
        render::Readback ticket = [&] {
            if (fxaa) {
                renderer.Submit(filtered.Handle(), FxaaPass(scene.Handle()));
                return renderer.RequestReadback(filtered.Handle());
            }
            return renderer.RequestReadback(scene.Handle());
        }();
        renderer.EndFrame();
        const auto image = Complete(renderer, std::move(ticket));
        Frame frame;
        for (std::size_t i = 0; i < image.rgba_.size(); i += 4) {
            const auto value = image.rgba_[i];
            frame.peak = std::max<int>(frame.peak, value);
            if (value > 8 && value < 247) ++frame.fractional;
            frame.energy += value;
            frame.luma.push_back(value);
        }
        frames.push_back(std::move(frame));
    }
    Metrics metrics;
    double mean_energy = 0;
    for (const auto& frame : frames) {
        metrics.mean_peak += frame.peak;
        metrics.mean_fractional += frame.fractional;
        mean_energy += frame.energy;
    }
    metrics.mean_peak /= kFrames;
    metrics.mean_fractional /= kFrames;
    mean_energy /= kFrames;
    double variance = 0;
    for (const auto& frame : frames)
        variance += (frame.energy - mean_energy) * (frame.energy - mean_energy);
    metrics.energy_cv = std::sqrt(variance / kFrames) / mean_energy;
    for (int frame_index = 1; frame_index < kFrames; ++frame_index) {
        std::uint64_t difference = 0;
        for (std::size_t i = 0; i < frames[0].luma.size(); ++i)
            difference += std::abs(int(frames[frame_index].luma[i]) -
                                   int(frames[frame_index - 1].luma[i]));
        metrics.flicker += double(difference) / frames[0].luma.size();
    }
    metrics.flicker /= (kFrames - 1);
    return metrics;
}
}  // namespace

void VerifyAntialiasingMotion(render::Renderer& renderer) {
    using namespace render;
    const std::array<std::uint8_t, 4> black_pixels{0, 0, 0, 255};
    const std::array<std::uint8_t, 4> white_pixels{255, 255, 255, 255};
    auto black = renderer.CreateTexture({1, 1}, black_pixels);
    auto white = renderer.CreateTexture({1, 1}, white_pixels);

    // Thin diagonal line (1.5 px wide, slope 0.1) sweeping 0.25 px per frame.
    const auto line = [&](int frame) {
        const float x = 20.0f + frame * 0.25f;
        return Scene(black.Handle(), white.Handle(), x, 8, x + 1.5f, 8, x + 6.3f, 56, x + 4.8f,
                     56);
    };
    // Particle-scale bright spot (1.5 px) drifting diagonally.
    const auto particle = [&](int frame) {
        const float x = 30.0f + frame * 0.25f;
        const float y = 30.0f + frame * 0.375f;
        return Scene(black.Handle(), white.Handle(), x, y, x + 1.5f, y, x + 1.5f, y + 1.5f, x,
                     y + 1.5f);
    };
    const struct {
        const char* name;
        Metrics plain;
        Metrics fxaa;
        double min_plain_flicker;
        double min_plain_cv;
        double min_frac_spread;
        double min_peak_drop;
    } results[] = {
        // The 1.5 px line keeps a fully lit core, so FXAA spreads the staircase
        // edge (fractional coverage) without lowering the peak.
        {"line", RunSeries(renderer, line, false), RunSeries(renderer, line, true), 0.5, 0, 40,
         0},
        // The 1.5 px spot has no interior, so FXAA visibly softens its peak.
        {"particle", RunSeries(renderer, particle, false), RunSeries(renderer, particle, true),
         0.05, 0.05, 2, 8}};
    for (const auto& result : results) {
        std::cout << "AA motion " << result.name << ": plain peak=" << result.plain.mean_peak
                  << " frac=" << result.plain.mean_fractional << " cv=" << result.plain.energy_cv
                  << " flicker=" << result.plain.flicker << " | fxaa peak=" << result.fxaa.mean_peak
                  << " frac=" << result.fxaa.mean_fractional << " cv=" << result.fxaa.energy_cv
                  << " flicker=" << result.fxaa.flicker << '\n';
    }
    for (const auto& result : results) {
        // Subpixel motion must actually exercise temporal instability.
        Check(result.plain.flicker > result.min_plain_flicker, "aa_motion.no_subpixel_flicker");
        Check(result.plain.energy_cv > result.min_plain_cv, "aa_motion.no_coverage_flicker");
        // FXAA softens the thin feature: fractional coverage grows, and
        // interior-free features lose peak intensity.
        Check(result.fxaa.mean_fractional >= result.plain.mean_fractional + result.min_frac_spread,
              "aa_motion.fxaa_did_not_spread");
        Check(result.fxaa.mean_peak <= result.plain.mean_peak - result.min_peak_drop,
              "aa_motion.fxaa_did_not_soften");
        // FXAA is spatial: temporal flicker largely remains (documented limit),
        // and it must not make flicker worse either.
        Check(result.fxaa.flicker >= result.plain.flicker * 0.3,
              "aa_motion.fxaa_unexpectedly_temporal");
        Check(result.fxaa.flicker <= result.plain.flicker * 1.5, "aa_motion.fxaa_worse_flicker");
    }
    std::cout << "AA motion: thin line and particle softening/flicker profile passed\n";
}
}  // namespace rhythm::validation
