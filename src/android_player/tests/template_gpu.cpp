#include <GLES3/gl3.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "rhythm/player/render_quality.h"
#include "rhythm/player/session.h"
#include "rhythm/render/renderer.h"

namespace rhythm::validation {
// Native offscreen test, not a display refresh or thermal endurance claim.
void MeasureTemplate(render::Renderer& renderer, const std::filesystem::path& path,
                     player::RenderQuality quality) {
    player::Session session;
    session.Open(path);
    const auto extent = player::PlaybackExtent(session.Canvas(), quality);
    std::vector<double> milliseconds;
    std::uint64_t baseline = 0;
    for (int frame = 0; frame < 420; ++frame) {
        runtime::ExternalInputs inputs{};
        auto& audio = inputs.audio_.emplace();
        audio.valid_ = true;
        audio.generation_ = 1;
        audio.sample_rate_ = 48000;
        audio.center_seconds_ = frame / 60.0;
        audio.rms_ = static_cast<float>(0.3 + 0.2 * std::sin(frame / 12.0));
        audio.loudness_ = audio.rms_;
        for (std::size_t band = 0; band < audio.mono_bands_.size(); ++band) {
            const auto index = static_cast<double>(band);
            const auto time = frame / 60.0;
            audio.mono_bands_[band] =
                    static_cast<float>((0.25 + 0.18 * std::sin(index * 0.36 + time * 2)) *
                                       (0.65 + 0.35 * std::sin(time * 5 + index * 0.08)));
        }
        const auto start = std::chrono::steady_clock::now();
        renderer.BeginFrame();
        const auto output = session.Tick(frame / 60.0, false, extent, renderer, inputs);
        if (output.budget_ || !renderer.IsValid(output.final_))
            throw std::runtime_error("template rejected or missing GPU output");
        renderer.EndFrame();
        glFinish();
        if (glGetError() != GL_NO_ERROR) throw std::runtime_error("template GPU error");
        if (frame == 120) baseline = renderer.Stats().texture_bytes_;
        if (frame >= 120) {
            if (renderer.Stats().texture_bytes_ != baseline)
                throw std::runtime_error("template texture growth");
            milliseconds.push_back(std::chrono::duration<double, std::milli>(
                                           std::chrono::steady_clock::now() - start)
                                           .count());
        }
    }
    std::sort(milliseconds.begin(), milliseconds.end());
    std::cout << "native_offscreen_frames=" << milliseconds.size() << " extent=" << extent.width_
              << 'x' << extent.height_
              << " synchronized_p50_ms=" << milliseconds[milliseconds.size() / 2]
              << " synchronized_p95_ms=" << milliseconds[milliseconds.size() * 95 / 100]
              << " stable_texture_bytes=" << baseline << '\n';
}
}  // namespace rhythm::validation
