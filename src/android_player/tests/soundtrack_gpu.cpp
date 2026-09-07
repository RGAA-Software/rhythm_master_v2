#include <GLES3/gl3.h>

#include <algorithm>
#include <array>
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
void VerifyMusicPackage(render::Renderer& renderer, const std::filesystem::path& path,
                        bool arrangement) {
    const auto frames = arrangement ? 960 : 240;
    const std::vector<int> checkpoints =
            arrangement ? std::vector<int>{120, 360, 600, 840} : std::vector<int>{239};
    std::array<std::vector<render::ReadbackImage>, 2> images;
    for (int silent = 0; silent < 2; ++silent) {
        player::Session session;
        session.Open(path);
        const auto track = session.Soundtrack();
        if (!track) throw std::runtime_error("music package missing soundtrack");
        auto decoder = track->file_bytes_.Valid() ? media::AudioDecoder(track->file_bytes_)
                                                  : media::AudioDecoder(track->bytes_);
        audio::Analyzer analyzer;
        if (!analyzer.Reset(48000, 1)) throw std::runtime_error("music analyzer");
        auto target = renderer.CreateTexture({320, 180});
        std::optional<media::AudioBlock> block;
        std::size_t offset = 0;
        std::uint64_t sample = 0, stable_bytes = 0;
        std::vector<double> elapsed;
        float maximum_rms = 0;
        std::optional<render::Readback> ticket;
        const auto collect = [&] {
            if (!ticket) return;
            if (auto image = ticket->Poll()) {
                const auto suffix =
                        arrangement ? "." + std::to_string(checkpoints[images[silent].size()] / 60)
                                    : std::string{};
                Write(std::filesystem::path(path.string() + suffix +
                                            (silent ? ".silence.ppm" : ".music.ppm")),
                      *image);
                images[silent].push_back(std::move(*image));
                ticket.reset();
            }
        };
        for (int frame = 0; frame < frames; ++frame) {
            const auto begin = std::chrono::steady_clock::now();
            if (frame) {
                std::vector<float> pcm(1600);
                for (auto& value : pcm) {
                    if (!block || offset == block->samples_.size()) {
                        block = decoder.Read();
                        offset = 0;
                    }
                    if (!block)
                        throw std::runtime_error("music fixture ended before requested duration");
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
            const bool capture =
                    std::find(checkpoints.begin(), checkpoints.end(), frame) != checkpoints.end();
            if (capture) {
                if (ticket) throw std::runtime_error("previous music readback pending");
                renderer.Submit(target.Handle(), Quad(output.final_), 0x000000ff);
                ticket.emplace(renderer.RequestReadback(target.Handle()));
            }
            renderer.EndFrame();
            glFinish();
            if (glGetError() != GL_NO_ERROR) throw std::runtime_error("music GLES error");
            if (frame == 60) stable_bytes = renderer.Stats().texture_bytes_;
            if (frame >= 60 && !ticket) {
                if (stable_bytes != renderer.Stats().texture_bytes_)
                    throw std::runtime_error("music texture growth");
                elapsed.push_back(std::chrono::duration<double, std::milli>(
                                          std::chrono::steady_clock::now() - begin)
                                          .count());
            }
            collect();
        }
        for (int frame = 0; frame < 12 && ticket; ++frame) {
            renderer.BeginFrame();
            renderer.EndFrame();
            collect();
        }
        if (images[silent].size() != checkpoints.size() || (!silent && maximum_rms < 0.01F))
            throw std::runtime_error("music readback/audio");
        std::sort(elapsed.begin(), elapsed.end());
        std::cout << (silent ? "silence" : "music") << " native_GLES_frames=" << frames
                  << " extent=640x360 p50_ms=" << elapsed[elapsed.size() / 2]
                  << " p95_ms=" << elapsed[elapsed.size() * 95 / 100]
                  << " texture_bytes=" << stable_bytes << " maximum_rms=" << maximum_rms << '\n';
    }
    for (std::size_t checkpoint = 0; checkpoint < checkpoints.size(); ++checkpoint) {
        const auto& music = images[0][checkpoint];
        const auto& silence = images[1][checkpoint];
        double difference = 0, brightness = 0;
        for (std::size_t offset = 0; offset < music.rgba_.size(); ++offset) {
            if (offset % 4 == 3) continue;
            brightness += music.rgba_[offset];
            difference += std::abs(int(music.rgba_[offset]) - int(silence.rgba_[offset]));
        }
        const auto count = music.rgba_.size() / 4 * 3;
        difference /= count;
        brightness /= count;
        std::cout << "packaged music vs silence mean_rgb_difference=" << difference
                  << " music_mean_rgb=" << brightness
                  << " seconds=" << checkpoints[checkpoint] / 60.0 << '\n';
        if (difference < (arrangement ? 0.15 : 1.0) || brightness < (arrangement ? 0.5 : 2.0))
            throw std::runtime_error("packaged music does not change GLES pixels");
    }
}
}  // namespace rhythm::validation
