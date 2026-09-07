#include <bgfx/bgfx.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string_view>

#include "blur_pass.h"
#include "rhythm/platform/host.h"
#include "rhythm/player/session.h"
#include "texture_ops.h"

namespace {
void Present(rhythm::render::Renderer& renderer, rhythm::render::TextureHandle image,
             rhythm::render::Extent extent) {
    rhythm::render::DrawList draw;
    draw.width_ = extent.width_;
    draw.height_ = extent.height_;
    rhythm::runtime::detail::AppendTextureQuad(draw, image, 0xffffffff, 0xffffffff);
    renderer.Submit({}, draw);
}
}  // namespace
int main(int argc, char* argv[]) {
    using namespace rhythm;
    try {
        if (argc < 2 || argc > 4) throw std::invalid_argument("effects.arguments");
        const std::filesystem::path output(argv[1]);
        platform::Host host(true);
        auto renderer = host.CreateRenderer();
        if (argc >= 3) {
            const auto mode = argc == 4 ? std::string_view(argv[3]) : std::string_view{};
            const bool silent = mode == "--silent";
            const bool benchmark = mode == "--benchmark";
            if (!mode.empty() && !silent && !benchmark)
                throw std::invalid_argument("effects.preview_mode");
            const render::Extent extent =
                    benchmark ? render::Extent{1280, 720} : render::Extent{960, 540};
            const auto frame_count = benchmark ? 720 : 180;
            std::vector<double> frame_times;
            std::uint64_t baseline_bytes = 0;
            std::uint64_t peak_bytes = 0;

            player::Session session;
            session.Open(argv[2]);
            runtime::ExternalInputs inputs;
            inputs.audio_.emplace();
            auto& audio = *inputs.audio_;
            audio.valid_ = true;
            audio.generation_ = 1;
            audio.sample_rate_ = 48000;
            // Explicit synthetic canonical features, for reproducible visual review.
            // This is not evidence of microphone/system-audio device capture.
            for (int frame = -3; frame < frame_count; ++frame) {
                const auto time = std::max(0, frame) / (benchmark ? 60.0 : 30.0);
                audio.center_seconds_ = time;
                audio.rms_ = silent ? 0.0f : static_cast<float>(0.3 + 0.2 * std::sin(time * 5));
                audio.loudness_ = audio.rms_;
                for (std::size_t band = 0; band < audio.mono_bands_.size(); ++band) {
                    const auto index = static_cast<double>(band);
                    audio.mono_bands_[band] =
                            silent ? 0.0f
                                   : static_cast<float>(
                                             (0.25 + 0.18 * std::sin(index * 0.36 + time * 2)) *
                                             (0.65 + 0.35 * std::sin(time * 5 + index * 0.08)));
                }
                const auto start = std::chrono::steady_clock::now();
                renderer.BeginFrame();
                const auto image = session.Tick(time, false, extent, renderer, inputs);
                Present(renderer, image.final_, extent);
                const auto path = (output / ("preview-" + std::to_string(frame))).string();
                if (frame >= 0 && !benchmark)
                    bgfx::requestScreenShot(BGFX_INVALID_HANDLE, path.c_str());
                renderer.EndFrame();
                if (benchmark && frame >= 120) {
                    frame_times.push_back(std::chrono::duration<double, std::milli>(
                                                  std::chrono::steady_clock::now() - start)
                                                  .count());
                    const auto bytes = renderer.Stats().texture_bytes_;
                    if (frame == 120) baseline_bytes = bytes;
                    peak_bytes = std::max(peak_bytes, bytes);
                }
            }
            if (benchmark) {
                std::sort(frame_times.begin(), frame_times.end());
                if (peak_bytes != baseline_bytes)
                    throw std::runtime_error("effects.texture_growth");
                std::cout << "benchmark_extent=1280x720 measured_frames=" << frame_times.size()
                          << " host_frame_p50_ms=" << frame_times[frame_times.size() / 2]
                          << " host_frame_p95_ms=" << frame_times[frame_times.size() * 95 / 100]
                          << " stable_texture_bytes=" << peak_bytes << '\n';
            }
            const auto stats = renderer.Stats();
            std::cout << "synthetic_frames=" << frame_count << " silent=" << silent
                      << " passes=" << stats.passes_ << " texture_bytes=" << stats.texture_bytes_
                      << '\n';
            return 0;
        }
        for (int scenario = 0; scenario < 4; ++scenario) {
            std::vector<std::uint8_t> pixels(64 * 64 * 4, 0);
            for (int y = 0; y < 64; ++y)
                for (int x = 0; x < 64; ++x) {
                    const auto offset = static_cast<std::size_t>((y * 64 + x) * 4);
                    if (scenario == 0) {
                        pixels[offset] = 100;
                        pixels[offset + 1] = 200;
                        pixels[offset + 2] = 40;
                        pixels[offset + 3] = 128;
                    } else if (x >= 30 && x < 34 && y >= 30 && y < 34) {
                        pixels[offset] = pixels[offset + 1] = pixels[offset + 2] = 255;
                        pixels[offset + 3] = 255;
                    } else {
                        // Transparent red must not bleed into a neutral halo.
                        pixels[offset] = 255;
                    }
                }
            auto source = renderer.CreateTexture({64, 64}, pixels);
            runtime::detail::BlurPass blur;
            const auto path = (output / ("blur-" + std::to_string(scenario))).string();
            for (int frame = 0; frame < 8; ++frame) {
                renderer.BeginFrame();
                const auto radius = scenario == 3 ? 0.0f : scenario == 2 ? 8.0f : 1.0f;
                const auto blurred = blur.Draw(source.Handle(), {64, 64}, radius, renderer);
                Present(renderer, blurred, {64, 64});
                if (frame == 3) bgfx::requestScreenShot(BGFX_INVALID_HANDLE, path.c_str());
                renderer.EndFrame();
            }
        }
        const std::array<std::uint8_t, 4> white_pixel{255, 255, 255, 255};
        auto white = renderer.CreateTexture({1, 1}, white_pixel);
        for (int scenario = 0; scenario < 5; ++scenario) {
            render::TextureNoise noise;
            noise.color_a_ = {0, 0, 0, 1};
            noise.color_b_ = {1, 1, 1, 1};
            noise.contrast_ = 3;
            noise.phase_ = scenario == 1 ? 0.7f : 0.0f;
            noise.seed_ = scenario == 2 ? 35.0f : 0.0f;
            if (scenario == 4) {
                noise.color_a_ = {1, 0, 0, 0};
                noise.color_b_ = {0, 1, 0, 0};
            }
            const auto path = (output / ("noise-" + std::to_string(scenario))).string();
            for (int frame = 0; frame < 8; ++frame) {
                renderer.BeginFrame();
                render::DrawList draw;
                draw.width_ = draw.height_ = 64;
                runtime::detail::AppendTextureQuad(draw, white.Handle(), 0xffffffff, 0xffffffff);
                draw.commands_.back().texture_noise_ = noise;
                renderer.Submit({}, draw);
                if (frame == 3) bgfx::requestScreenShot(BGFX_INVALID_HANDLE, path.c_str());
                renderer.EndFrame();
            }
        }
        for (int scenario = 0; scenario < 5; ++scenario) {
            std::vector<std::uint8_t> pixels(64 * 64 * 4);
            for (int y = 0; y < 64; ++y)
                for (int x = 0; x < 64; ++x) {
                    const auto offset = static_cast<std::size_t>((y * 64 + x) * 4);
                    pixels[offset] = pixels[offset + 1] = pixels[offset + 2] =
                            scenario == 0 ? 120 : static_cast<std::uint8_t>(x * 4);
                    pixels[offset + 3] = scenario == 4 ? 0 : 255;
                }
            auto source = renderer.CreateTexture({64, 64}, pixels);
            const auto path = (output / ("mapping-" + std::to_string(scenario))).string();
            for (int frame = 0; frame < 8; ++frame) {
                renderer.BeginFrame();
                render::DrawList draw;
                draw.width_ = draw.height_ = 64;
                runtime::detail::AppendTextureQuad(draw, source.Handle(), 0xffffffff, 0xffffffff);
                if (scenario < 3) {
                    render::TextureMapping mapping;
                    mapping.sectors_ = 4;
                    if (scenario == 2) {
                        mapping.kind_ = render::TextureMappingKind::kPolar;
                        mapping.radial_power_ = -2;
                        mapping.twist_ = 16;
                    }
                    draw.commands_.back().texture_mapping_ = mapping;
                } else {
                    render::TextureContours contours;
                    contours.count_ = 8;
                    contours.color_a_ = contours.color_b_ = {1, 1, 1, 1};
                    draw.commands_.back().texture_contours_ = contours;
                }
                renderer.Submit({}, draw);
                if (frame == 3) bgfx::requestScreenShot(BGFX_INVALID_HANDLE, path.c_str());
                renderer.EndFrame();
            }
        }
        graph::Registry registry;
        graph::Document document;
        document.id_ = "cached-resize";
        document.nodes_ = {registry.MakeNode(1, "texture.gradient"),
                           registry.MakeNode(2, "output.texture")};
        document.nodes_[0].properties_["color_a"] = graph::Color{0, 1, 0, 1};
        document.nodes_[0].properties_["color_b"] = graph::Color{0, 1, 0, 1};
        document.edges_ = {{1, 1, 2, "source"}};
        document.output_ = 2;
        const auto plan = std::get<graph::ExecutionPlan>(graph::Compile(document, registry));
        runtime::Runtime runtime;
        for (int frame = 0; frame < 8; ++frame) {
            renderer.BeginFrame();
            const auto image = runtime.Evaluate(plan, {.extent_ = {64, 64}}, renderer);
            Present(renderer, image.final_, {96, 64});
            if (frame == 5) {
                const auto path = (output / "cached-resize").string();
                bgfx::requestScreenShot(BGFX_INVALID_HANDLE, path.c_str());
            }
            renderer.EndFrame();
        }
        std::cout << "Blur GPU captures: constant, impulse, pyramid, bypass\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
