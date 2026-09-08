#include <bgfx/bgfx.h>

#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "rhythm/audio/analyzer.h"
#include "rhythm/media/audio_decoder.h"
#include "rhythm/platform/host.h"
#include "rhythm/player/session.h"
#include "rhythm/project/package.h"

namespace {
std::vector<rhythm::audio::Features> Decode(const std::filesystem::path& path) {
    // Offline fixture preparation completes before the graphics host exists.
    rhythm::media::AudioDecoder decoder(path);
    rhythm::audio::Analyzer analyzer;
    if (!analyzer.Reset(48000, 1)) throw std::runtime_error("music.analysis_reset");
    std::vector<rhythm::audio::Features> frames;
    while (const auto block = decoder.Read()) {
        if (!analyzer.Push(block->samples_, 2, block->first_sample_))
            throw std::runtime_error("music.analysis_input");
        const auto frame = analyzer.Snapshot();
        if (frame.valid_) frames.push_back(frame);
    }
    if (frames.empty()) throw std::runtime_error("music.analysis_empty");
    return frames;
}
}  // namespace

int main(int argc, char* argv[]) {
    using namespace rhythm;
    try {
        if (argc != 4 && argc != 5)
            throw std::invalid_argument("music_gpu package fixtures output [expected_nodes]");
        const auto expected_nodes = argc == 5 ? std::stoull(argv[4]) : 164;
        const auto package = project::LoadPackage(argv[1]);
        const auto& instructions = package.program_.instructions_;
        const auto bands = std::count_if(
                instructions.begin(), instructions.end(), [](const auto& instruction) {
                    return instruction.operation_ == graph::Operation::kAudioBand;
                });
        const auto instance_fields = std::count_if(
                instructions.begin(), instructions.end(), [](const auto& instruction) {
                    return instruction.operation_ == graph::Operation::kPointInstances;
                });
        if (instructions.size() != expected_nodes || (bands < 24 && instance_fields == 0))
            throw std::runtime_error("music.full_graph_not_reachable");
        std::cout << "reachable_instructions=" << instructions.size() << " audio_bands=" << bands
                  << " spectral_instance_fields=" << instance_fields << '\n';
        const std::filesystem::path fixtures(argv[2]), output(argv[3]);
        const std::array<std::string, 4> names{"resonance_demo", "silence", "low", "high"};
        std::array<std::vector<audio::Features>, 4> features;
        for (std::size_t index = 0; index < names.size(); ++index)
            features[index] = Decode(fixtures / (names[index] + ".wav"));
        if (features[1].back().rms_ != 0 || features[2].back().rms_ < 0.1f ||
            features[3].back().rms_ < 0.1f)
            throw std::runtime_error("music.fixture_amplitude");
        platform::Host host(true);
        host.Resize({1280, 720});
        auto renderer = host.CreateRenderer();
        for (std::size_t scenario = 0; scenario < names.size(); ++scenario) {
            player::Session session;
            session.Open(argv[1]);
            runtime::ExternalInputs inputs;
            std::size_t cursor = 0;
            std::uint64_t stable_bytes = 0;
            for (int frame = 0; frame < 124; ++frame) {
                const auto seconds = std::min(frame, 120) / 30.0;
                const auto& sequence = features[scenario];
                while (cursor + 1 < sequence.size() &&
                       sequence[cursor + 1].center_seconds_ <= seconds)
                    ++cursor;
                if (sequence[cursor].center_seconds_ <= seconds) inputs.audio_ = sequence[cursor];
                renderer.BeginFrame();
                const auto image = session.Tick(seconds, false, {1280, 720}, renderer, inputs);
                render::DrawList draw;
                draw.width_ = 1280;
                draw.height_ = 720;
                draw.vertices_ = {{0, 0, 0, 0}, {1280, 0, 1, 0}, {1280, 720, 1, 1}, {0, 720, 0, 1}};
                draw.indices_ = {0, 1, 2, 0, 2, 3};
                draw.commands_ = {{image.final_, 0, 6, {0, 0, 1280, 720}}};
                renderer.Submit({}, draw);
                if (frame == 120) {
                    const auto path = (output / names[scenario]).string();
                    bgfx::requestScreenShot(BGFX_INVALID_HANDLE, path.c_str());
                }
                renderer.EndFrame();
                const auto bytes = renderer.Stats().texture_bytes_;
                if (frame == 30) stable_bytes = bytes;
                if (frame > 30 && bytes != stable_bytes)
                    throw std::runtime_error("music.texture_growth");
            }
            std::cout << names[scenario] << " decoded_feature_frames=" << features[scenario].size()
                      << " texture_bytes=" << stable_bytes << '\n';
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
