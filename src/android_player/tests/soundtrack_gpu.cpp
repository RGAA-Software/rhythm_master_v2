#include <GLES3/gl3.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>

#include "rhythm/audio/analyzer.h"
#include "rhythm/media/audio_decoder.h"
#include "rhythm/player/session.h"

namespace rhythm::validation {
namespace {
render::DrawList Quad(render::TextureHandle texture) {
    render::DrawList list;
    list.width_ = 320;
    list.height_ = 180;
    list.vertices_ = {{0, 0, 0, 0}, {320, 0, 1, 0}, {320, 180, 1, 1}, {0, 180, 0, 1}};
    list.indices_ = {0, 1, 2, 0, 2, 3};
    list.commands_ = {{texture, 0, 6, {0, 0, 320, 180}}};
    return list;
}
void Write(const std::filesystem::path& path, const render::ReadbackImage& image) {
    std::ofstream file(path, std::ios::binary);
    file.exceptions(std::ios::badbit | std::ios::failbit);
    file << "P6\n320 180\n255\n";
    for (std::size_t offset = 0; offset < image.rgba_.size(); offset += 4)
        file.write(reinterpret_cast<const char*>(image.rgba_.data() + offset), 3);
}
}  // namespace
// Actual packaged PCM, shared analysis and Session on GLES. This offscreen
// diagnostic does not substitute for Android application audio/lifecycle tests.
void VerifyMusicPackage(render::Renderer& renderer, const std::filesystem::path& path) {
    std::vector<render::ReadbackImage> images;
    for (int silent = 0; silent < 2; ++silent) {
        player::Session session;
        session.Open(path);
        const auto track = session.Soundtrack();
        if (!track) throw std::runtime_error("music package missing soundtrack");
        media::AudioDecoder decoder(track->bytes_);
        audio::Analyzer analyzer;
        if (!analyzer.Reset(48000, 1)) throw std::runtime_error("music analyzer");
        auto target = renderer.CreateTexture({320, 180});
        std::optional<media::AudioBlock> block;
        std::size_t offset = 0;
        std::uint64_t sample = 0, stable_bytes = 0;
        std::vector<double> elapsed;
        float maximum_rms = 0;
        std::optional<render::Readback> ticket;
        for (int frame = 0; frame < 240; ++frame) {
            const auto begin = std::chrono::steady_clock::now();
            if (frame) {
                std::vector<float> pcm(1600);
                for (auto& value : pcm) {
                    if (!block || offset == block->samples_.size()) {
                        block = decoder.Read();
                        offset = 0;
                    }
                    if (!block) throw std::runtime_error("music test needs four seconds of audio");
                    value = silent ? 0 : block->samples_[offset];
                    ++offset;
                }
                if (!analyzer.Push(pcm, 2, sample))
                    throw std::runtime_error("music analysis continuity");
                sample += 800;
            }
            runtime::ExternalInputs inputs;
            inputs.audio_ = analyzer.Snapshot();
            maximum_rms = std::max(maximum_rms, inputs.audio_->rms_);
            renderer.BeginFrame();
            const auto output = session.Tick(frame / 60.0, false, {640, 360}, renderer, inputs,
                                             runtime::PlaybackSample{frame / 60.0, 1, false, 16});
            if (output.budget_ || !renderer.IsValid(output.final_))
                throw std::runtime_error("music scene budget/output");
            if (frame == 239) {
                renderer.Submit(target.Handle(), Quad(output.final_), 0x000000ff);
                ticket.emplace(renderer.RequestReadback(target.Handle()));
            }
            renderer.EndFrame();
            glFinish();
            if (glGetError() != GL_NO_ERROR) throw std::runtime_error("music GLES error");
            if (frame == 60) stable_bytes = renderer.Stats().texture_bytes_;
            if (frame >= 60 && frame < 239) {
                if (stable_bytes != renderer.Stats().texture_bytes_)
                    throw std::runtime_error("music texture growth");
                elapsed.push_back(std::chrono::duration<double, std::milli>(
                                          std::chrono::steady_clock::now() - begin)
                                          .count());
            }
        }
        std::optional<render::ReadbackImage> image;
        for (int frame = 0; frame < 12 && !image; ++frame) {
            renderer.BeginFrame();
            renderer.EndFrame();
            image = ticket->Poll();
        }
        if (!image || (!silent && maximum_rms < 0.01F))
            throw std::runtime_error("music readback/audio");
        Write(std::filesystem::path(path.string() + (silent ? ".silence.ppm" : ".music.ppm")),
              *image);
        images.push_back(std::move(*image));
        std::sort(elapsed.begin(), elapsed.end());
        std::cout << (silent ? "silence" : "music")
                  << " native_GLES_frames=240 extent=640x360 p50_ms=" << elapsed[elapsed.size() / 2]
                  << " p95_ms=" << elapsed[elapsed.size() * 95 / 100]
                  << " texture_bytes=" << stable_bytes << " maximum_rms=" << maximum_rms << '\n';
    }
    double difference = 0, brightness = 0;
    for (std::size_t offset = 0; offset < images[0].rgba_.size(); ++offset) {
        if (offset % 4 == 3) continue;
        brightness += images[0].rgba_[offset];
        difference += std::abs(int(images[0].rgba_[offset]) - int(images[1].rgba_[offset]));
    }
    const auto count = images[0].rgba_.size() / 4 * 3;
    difference /= count;
    brightness /= count;
    if (difference < 1 || brightness < 2)
        throw std::runtime_error("packaged music does not change GLES pixels");
    std::cout << "packaged music vs silence mean_rgb_difference=" << difference
              << " music_mean_rgb=" << brightness << '\n';
}
}  // namespace rhythm::validation
