#include "scene_compositor_probe.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "rhythm/player/scene_compositor.h"
#include "rhythm/player/scene_deck.h"

namespace rhythm::validation {
namespace {
void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
render::ReadbackImage Complete(render::Renderer& renderer, render::Readback& readback) {
    for (int index = 0; index < 16; ++index) {
        if (auto image = readback.Poll()) return std::move(*image);
        renderer.BeginFrame();
        renderer.EndFrame();
    }
    throw std::runtime_error("scene dissolve readback deadline");
}
}  // namespace
void VerifySceneCompositor(render::Renderer& renderer) {
    const auto baseline = renderer.Stats().texture_bytes_;
    {
        const std::array<std::uint8_t, 4> red{200, 0, 0, 128}, green{0, 200, 0, 64};
        auto first = renderer.CreateTexture({1, 1}, red);
        auto second = renderer.CreateTexture({1, 1}, green);
        player::SceneCompositor compositor;
        const auto stable = baseline + 8 + 8 * 8 * 4;
        render::TextureHandle previous;
        for (const auto amount : {0.0, 0.5, 1.0}) {
            renderer.BeginFrame();
            const auto output = compositor.Blend(renderer, {first.Handle(), {8, 8}},
                                                 {second.Handle(), {4, 8}}, {8, 8}, amount);
            Check(previous == render::TextureHandle{} || previous == output,
                  "dissolve reuses its target");
            previous = output;
            Check(renderer.Stats().texture_bytes_ == stable, "one additional dissolve target");
            auto readback = renderer.RequestReadback(output);
            renderer.EndFrame();
            const auto image = Complete(renderer, readback);
            const auto weight = std::lround(amount * 255) / 255.0;
            for (int y = 1; y < 7; ++y) {
                for (const int x : {0, 3, 4, 7}) {
                    const bool inside = x >= 2 && x < 6;
                    const std::array<double, 4> expected{
                            100 * (1 - weight), inside ? 50 * weight : 0, 0,
                            128 * (1 - weight) + (inside ? 64 * weight : 0)};
                    const auto offset = static_cast<std::size_t>(y * 8 + x) * 4;
                    for (std::size_t channel = 0; channel < 4; ++channel)
                        Check(std::abs(image.rgba_[offset + channel] - expected[channel]) <= 2,
                              "dissolve endpoint/midpoint/alpha/aspect pixels");
                }
            }
        }
        renderer.BeginFrame();
        bool rejected = false;
        try {
            compositor.Blend(renderer, {first.Handle(), {8, 8}}, {second.Handle(), {4, 8}}, {8, 8},
                             std::numeric_limits<double>::quiet_NaN());
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        Check(rejected, "reject invalid dissolve without replacing target");
        renderer.EndFrame();
        compositor.ReleaseGraphics();
        Check(renderer.Stats().texture_bytes_ == baseline + 8, "dissolve target released");
    }
    Check(renderer.Stats().texture_bytes_ == baseline, "scene dissolve resources released");
    std::cout << "Scene dissolve: endpoints, midpoint, premultiplied alpha, aspect fit and reuse "
                 "pass\n";
}
void VerifySceneDeck(render::Renderer& renderer, const std::filesystem::path& first,
                     const std::filesystem::path& second) {
    const auto baseline = renderer.Stats().texture_bytes_;
    player::SceneDeck deck;
    deck.Open(first);
    player::PreparedPackage incoming(
            storage::FileBytes::Open(second, project::kMaximumFilePackageBytes));
    const auto first_title = deck.Current().Title();
    bool switched = false;
    std::uint64_t peak_bytes = baseline;
    std::uint32_t peak_passes = 0;
    std::vector<double> cpu_ms;
    std::vector<double> frame_ms;
    for (int frame = 0; frame < 100; ++frame) {
        const auto started = std::chrono::steady_clock::now();
        const auto previous_bytes = renderer.Stats().texture_bytes_;
        if (frame == 10)
            Check(deck.StartTransition(std::move(incoming), 1), "complex transition starts");
        runtime::ExternalInputs inputs;
        auto& audio = inputs.audio_.emplace();
        audio.valid_ = true;
        audio.generation_ = 1;
        audio.sample_rate_ = 48000;
        audio.center_seconds_ = frame / 60.0;
        audio.rms_ = audio.loudness_ = 0.45f;
        audio.mono_bands_.fill(0.4f);
        renderer.BeginFrame();
        const bool was_preparing = deck.PreparingGraphics();
        const auto result =
                deck.Tick(frame / 60.0, false, player::RenderQuality::kBalanced, renderer, inputs);
        const double elapsed = std::chrono::duration<double, std::milli>(
                                       std::chrono::steady_clock::now() - started)
                                       .count();
        const bool preparation_frame = was_preparing || deck.PreparingGraphics();
        if (preparation_frame) {
            const auto& preparation = deck.GraphicsPreparation();
            std::cout << "preparation_frame=" << frame << " nodes=" << preparation.completed_nodes_
                      << '/' << preparation.total_nodes_ << " state=" << int(preparation.state_)
                      << " step_cpu_ms=" << preparation.cpu_ms_
                      << " maximum_node_ms=" << preparation.maximum_node_ms_
                      << " tick_cpu_ms=" << elapsed << " texture_delta_bytes="
                      << static_cast<std::int64_t>(renderer.Stats().texture_bytes_) -
                                 static_cast<std::int64_t>(previous_bytes)
                      << " passes=" << renderer.Stats().passes_ << '\n';
        }
        if (frame >= 10 && frame < 70) cpu_ms.push_back(elapsed);
        if (frame == 0 || frame == 10)
            std::cout << "scene_creation frame=" << frame << " cpu_submit_ms=" << elapsed
                      << " texture_delta_bytes="
                      << static_cast<std::int64_t>(renderer.Stats().texture_bytes_) -
                                 static_cast<std::int64_t>(previous_bytes)
                      << " passes=" << renderer.Stats().passes_ << '\n';
        Check(!result.output_.budget_ && renderer.IsValid(result.output_.final_),
              "complex scene valid output");
        Check(deck.Error() == player::SceneTransitionError::kNone, "complex transition no failure");
        switched |= result.switched_;
        peak_bytes = std::max(peak_bytes, renderer.Stats().texture_bytes_);
        peak_passes = std::max(peak_passes, renderer.Stats().passes_);
        if (frame == 40 || frame == 99) {
            auto readback = renderer.RequestReadback(result.output_.final_);
            renderer.EndFrame();
            const auto pixels = Complete(renderer, readback);
            std::size_t lit = 0;
            for (std::size_t offset = 0; offset < pixels.rgba_.size(); offset += 4)
                if (pixels.rgba_[offset] + pixels.rgba_[offset + 1] + pixels.rgba_[offset + 2] > 24)
                    ++lit;
            Check(lit > pixels.rgba_.size() / 400, "complex transition visible pixels");
            std::cout << "scene_frame=" << frame << " lit_pixels=" << lit << '\n';
        } else {
            renderer.EndFrame();
            const double ended = std::chrono::duration<double, std::milli>(
                                         std::chrono::steady_clock::now() - started)
                                         .count();
            if (frame >= 10 && frame < 70) frame_ms.push_back(ended);
            if (preparation_frame)
                std::cout << "preparation_frame=" << frame << " including_end_frame_ms=" << ended
                          << '\n';
            if (frame == 0 || frame == 10)
                std::cout << "scene_creation frame=" << frame << " including_end_frame_ms=" << ended
                          << '\n';
        }
    }
    Check(switched && !deck.Transitioning() && deck.Current().Title() != first_title,
          "complex incoming work takes over");
    deck.ReleaseGraphics();
    Check(renderer.Stats().texture_bytes_ == baseline, "complex scene resources released");
    std::cout << "complex_scene_frames=100 peak_texture_bytes=" << peak_bytes
              << " peak_passes=" << peak_passes << '\n';
    std::sort(cpu_ms.begin(), cpu_ms.end());
    std::cout << "dual_scene_cpu_submit p50_ms=" << cpu_ms[cpu_ms.size() / 2]
              << " p95_ms=" << cpu_ms[cpu_ms.size() * 95 / 100] << " max_ms=" << cpu_ms.back()
              << " (not GPU timings or UI FPS)\n";
    std::sort(frame_ms.begin(), frame_ms.end());
    std::cout << "dual_scene_including_end_frame p50_ms=" << frame_ms[frame_ms.size() / 2]
              << " p95_ms=" << frame_ms[frame_ms.size() * 95 / 100] << " max_ms=" << frame_ms.back()
              << " (readback frames excluded)\n";
}
}  // namespace rhythm::validation
