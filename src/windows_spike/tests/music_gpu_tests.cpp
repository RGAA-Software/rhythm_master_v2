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
        graph::NodeId onset_source = 0;
        for (const auto& instruction : instructions)
            if (instruction.operation_ == graph::Operation::kEventAudio)
                onset_source = instruction.node_.id_;
        const bool videos =
                std::any_of(instructions.begin(), instructions.end(), [](const auto& instruction) {
                    // Both texture.video and texture.video_clip compile to this operation.
                    return instruction.operation_ == graph::Operation::kTextureVideo;
                });
        const auto bands = std::count_if(
                instructions.begin(), instructions.end(), [](const auto& instruction) {
                    return instruction.operation_ == graph::Operation::kAudioBand;
                });
        const auto instance_fields = std::count_if(
                instructions.begin(), instructions.end(), [](const auto& instruction) {
                    return instruction.operation_ == graph::Operation::kPointInstances;
                });
        const auto gpu_fields = std::count_if(
                instructions.begin(), instructions.end(), [](const auto& instruction) {
                    return instruction.operation_ == graph::Operation::kGpuParticleEmitter;
                });
        const auto materials = std::count_if(
                instructions.begin(), instructions.end(), [](const auto& instruction) {
                    return instruction.operation_ == graph::Operation::kMaterialTextures;
                });
        const auto scene_instances = std::count_if(
                instructions.begin(), instructions.end(), [](const auto& instruction) {
                    return instruction.operation_ == graph::Operation::kSceneInstance;
                });
        const auto paths = std::count_if(
                instructions.begin(), instructions.end(), [](const auto& instruction) {
                    return instruction.operation_ == graph::Operation::kGeometryTube;
                });
        const auto deformations = std::count_if(
                instructions.begin(), instructions.end(), [](const auto& instruction) {
                    return instruction.operation_ == graph::Operation::kGeometryDeform;
                });
        const auto shaders = std::count_if(
                instructions.begin(), instructions.end(), [](const auto& instruction) {
                    return instruction.operation_ == graph::Operation::kTextureShader;
                });
        const auto image_filters = std::count_if(
                instructions.begin(), instructions.end(), [](const auto& instruction) {
                    using enum graph::Operation;
                    return instruction.operation_ == kTextureDisplace ||
                           instruction.operation_ == kTextureMapping ||
                           instruction.operation_ == kTextureContours ||
                           instruction.operation_ == kTextureTrail ||
                           instruction.operation_ == kGaussianBlur;
                });
        const auto animated_models = std::count_if(
                instructions.begin(), instructions.end(), [](const auto& instruction) {
                    return instruction.operation_ == graph::Operation::kGeometryAnimate ||
                           instruction.operation_ == graph::Operation::kGeometryMorph;
                });
        if (instructions.size() != expected_nodes ||
            (bands < 24 && instance_fields == 0 &&
             !((videos || gpu_fields > 0 || materials > 0 || scene_instances > 0 || paths > 0 ||
                deformations > 0 || shaders > 0 || animated_models > 0 || image_filters > 0) &&
               bands >= 2)))
            throw std::runtime_error("music.full_graph_not_reachable");
        std::cout << "reachable_instructions=" << instructions.size() << " audio_bands=" << bands
                  << " gpu_particle_fields=" << gpu_fields
                  << " spectral_instance_fields=" << instance_fields << " tube_meshes=" << paths
                  << " animated_model_nodes=" << animated_models << " synchronous_video=" << videos
                  << '\n';
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
            if (!videos) session.Open(argv[1]);
            // Resolve video on this offline test worker so comparisons hold
            // source frames fixed; decoder scheduling cannot mimic audio response.
            const auto resources =
                    videos ? prepared_assets::Prepare(package.program_, package.assets_)
                           : std::shared_ptr<const prepared_assets::Resources>{};
            video_sources::Streams video_streams;
            runtime::Runtime offline;
            runtime::ExternalInputs inputs;
            std::size_t cursor = 0;
            std::uint64_t stable_bytes = 0;
            std::uint64_t onset_events = 0;
            for (int frame = 0; frame < 124; ++frame) {
                const auto seconds = std::min(frame, 120) / 30.0;
                const auto& sequence = features[scenario];
                while (cursor + 1 < sequence.size() &&
                       sequence[cursor + 1].center_seconds_ <= seconds)
                    ++cursor;
                if (sequence[cursor].center_seconds_ <= seconds) inputs.audio_ = sequence[cursor];
                renderer.BeginFrame();
                runtime::FrameResult image;
                if (videos) {
                    runtime::FrameContext context{seconds, 1, {1280, 720}, false};
                    context.resources_ = resources->models_;
                    context.images_ = resources->images_;
                    context.shaders_ = resources->shaders_;
                    context.videos_ =
                            video_streams.Resolve(package.program_, *resources, seconds, 1);
                    context.external_ = inputs;
                    context.external_.controls_ = parameters::EvaluateControls(
                            package.program_.controls_, package.program_.control_sequence_,
                            seconds);
                    context.retained_textures_ = std::vector<graph::NodeId>{};
                    image = offline.Evaluate(package.program_, context, renderer);
                } else {
                    image = session.Tick(seconds, false, {1280, 720}, renderer, inputs);
                }
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
                for (const auto& value : image.outputs_)
                    if (value.node_ == onset_source && value.event_observation_)
                        onset_events = value.event_observation_->count_;
                if (image.rejected_event_total_) throw std::runtime_error("music.event_rejections");
                const auto bytes = renderer.Stats().texture_bytes_;
                if (frame == 30) stable_bytes = bytes;
                if (frame > 30 && bytes != stable_bytes)
                    throw std::runtime_error("music.texture_growth");
            }
            std::cout << names[scenario] << " decoded_feature_frames=" << features[scenario].size()
                      << " texture_bytes=" << stable_bytes << " audio_onset_events=" << onset_events
                      << '\n';
            if (onset_source &&
                ((scenario == 0 && onset_events == 0) || (scenario == 1 && onset_events != 0)))
                throw std::runtime_error("music.onset_event_response");
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
