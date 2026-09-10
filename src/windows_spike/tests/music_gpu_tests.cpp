#include <bgfx/bgfx.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string_view>
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
        if (argc < 4 || argc > 6 ||
            (argc == 6 && std::string_view(argv[5]) != "--quality" &&
             std::string_view(argv[5]) != "--control-extremes" &&
             std::string_view(argv[5]) != "--motion"))
            throw std::invalid_argument(
                    "music_gpu package fixtures output [expected_nodes "
                    "[--quality|--control-extremes|--motion]]");
        const auto expected_nodes = argc >= 5 ? std::stoull(argv[4]) : 164;
        const bool quality = argc == 6 && std::string_view(argv[5]) == "--quality";
        const bool controls = argc == 6 && std::string_view(argv[5]) == "--control-extremes";
        const bool motion = argc == 6 && std::string_view(argv[5]) == "--motion";
        const auto package = project::LoadPackage(argv[1]);
        const auto& instructions = package.program_.instructions_;
        std::map<graph::NodeId, std::uint64_t> onset_sources;
        for (const auto& instruction : instructions)
            if (instruction.operation_ == graph::Operation::kEventAudio)
                onset_sources[instruction.node_.id_] = 0;
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
        const auto text_nodes = std::count_if(
                instructions.begin(), instructions.end(), [](const auto& instruction) {
                    return instruction.operation_ == graph::Operation::kTextureText;
                });
        if (instructions.size() != expected_nodes ||
            (bands < 24 && instance_fields == 0 &&
             !((videos || gpu_fields > 0 || materials > 0 || scene_instances > 0 || paths > 0 ||
                deformations > 0 || shaders > 0 || animated_models > 0 || image_filters > 0 ||
                text_nodes > 0) &&
               bands >= 2)))
            throw std::runtime_error("music.full_graph_not_reachable");
        std::cout << "reachable_instructions=" << instructions.size() << " audio_bands=" << bands
                  << " gpu_particle_fields=" << gpu_fields
                  << " spectral_instance_fields=" << instance_fields << " tube_meshes=" << paths
                  << " animated_model_nodes=" << animated_models << " synchronous_video=" << videos
                  << '\n';
        const std::filesystem::path fixtures(argv[2]), output(argv[3]);
        std::vector<std::string> names{"resonance_demo", "silence", "low", "high"};
        if (motion) names.resize(2);
        if (quality) names.insert(names.end(), {"mid", "quiet", "loud"});
        if (controls) {
            if (package.program_.controls_.Definitions().empty())
                throw std::runtime_error("music.no_public_controls");
            std::ofstream manifest(output / "control-extremes.json");
            manifest.exceptions(std::ios::badbit | std::ios::failbit);
            manifest << "[";
            for (const auto& control : package.program_.controls_.Definitions()) {
                if (names.size() > 4) manifest << ",";
                const auto prefix = "control_" + std::to_string(control.id_);
                names.push_back(prefix + "_minimum");
                names.push_back(prefix + "_maximum");
                manifest << "{\"id\":" << control.id_ << ",\"minimum\":" << control.minimum_
                         << ",\"maximum\":" << control.maximum_ << "}";
            }
            manifest << "]\n";
        }
        std::vector<std::vector<audio::Features>> features(names.size());
        for (std::size_t index = 0; index < names.size(); ++index)
            features[index] =
                    Decode(fixtures /
                           ((controls && index >= 4 ? "resonance_demo" : names[index]) + ".wav"));
        if (features[1].back().rms_ != 0 ||
            (!motion && (features[2].back().rms_ < 0.1f || features[3].back().rms_ < 0.1f)))
            throw std::runtime_error("music.fixture_amplitude");
        if (quality) {
            const auto peak = [](const std::vector<audio::Features>& sequence) {
                float maximum = 0;
                for (const auto& frame : sequence) maximum = std::max(maximum, frame.rms_);
                return maximum;
            };
            if (peak(features[4]) < 0.1f || peak(features[5]) <= 0 ||
                peak(features[5]) >= peak(features[0]) || peak(features[6]) <= peak(features[0]))
                throw std::runtime_error("music.quality_fixture_amplitude");
        }
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
            if (controls && scenario >= 4) {
                const auto control = package.program_.controls_.Definitions()[(scenario - 4) / 2];
                // Hold every other public control at its authored default, so
                // Cue changes cannot masquerade as a response to this slider.
                inputs.controls_ = package.program_.controls_.Resolve();
                inputs.controls_[control.id_] =
                        scenario % 2 == 0 ? control.minimum_ : control.maximum_;
            }
            std::size_t cursor = 0;
            std::uint64_t stable_bytes = 0;
            std::ofstream trajectory;
            if (motion) {
                trajectory.open(output / (names[scenario] + "-camera.csv"));
                trajectory.exceptions(std::ios::badbit | std::ios::failbit);
                trajectory << "frame,node,eye_x,eye_y,eye_z,target_x,target_y,target_z\n";
            }
            for (auto& [id, count] : onset_sources) count = 0;
            for (int frame = 0; frame < (motion ? 964 : 124); ++frame) {
                const auto seconds = std::min(frame, motion ? 960 : 120) / 30.0;
                const auto audio_seconds = motion ? std::fmod(seconds, 4.0) : seconds;
                if (motion && frame % 120 == 0) cursor = 0;
                const auto& sequence = features[scenario];
                while (cursor + 1 < sequence.size() &&
                       sequence[cursor + 1].center_seconds_ <= audio_seconds)
                    ++cursor;
                if (sequence[cursor].center_seconds_ <= audio_seconds)
                    inputs.audio_ = sequence[cursor];
                renderer.BeginFrame();
                runtime::FrameResult image;
                if (videos) {
                    runtime::FrameContext context{seconds, 1, {1280, 720}, false};
                    context.resources_ = resources->models_;
                    context.images_ = resources->images_;
                    context.shaders_ = resources->shaders_;
                    context.surfaces_ = resources->surfaces_;
                    context.videos_ =
                            video_streams.Resolve(package.program_, *resources, seconds, 1);
                    context.external_ = inputs;
                    context.external_.controls_ = parameters::EvaluateControls(
                            package.program_.controls_, package.program_.control_sequence_, seconds,
                            inputs.controls_);
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
                const bool checkpoint = frame <= 960 && (frame % 60 == 0 || frame == 479 ||
                                                         frame == 481 || frame == 959);
                if ((!motion && frame == 120) || (motion && checkpoint)) {
                    const auto path = (output / (names[scenario] +
                                                 (motion ? "-" + std::to_string(frame) : "")))
                                              .string();
                    bgfx::requestScreenShot(BGFX_INVALID_HANDLE, path.c_str());
                }
                if (motion && frame <= 960) {
                    for (const auto& value : image.outputs_) {
                        if (!value.camera_) continue;
                        const auto& camera = *value.camera_;
                        trajectory << frame << ',' << value.node_ << ',' << camera.eye_.x_ << ','
                                   << camera.eye_.y_ << ',' << camera.eye_.z_ << ','
                                   << camera.target_.x_ << ',' << camera.target_.y_ << ','
                                   << camera.target_.z_ << '\n';
                    }
                }
                renderer.EndFrame();
                for (const auto& value : image.outputs_)
                    if (onset_sources.contains(value.node_) && value.event_observation_)
                        onset_sources[value.node_] = value.event_observation_->count_;
                if (image.rejected_event_total_) throw std::runtime_error("music.event_rejections");
                const auto bytes = renderer.Stats().texture_bytes_;
                if (frame == 30) stable_bytes = bytes;
                if (frame > 30 && bytes != stable_bytes)
                    throw std::runtime_error("music.texture_growth");
            }
            std::uint64_t onset_events = 0;
            for (const auto& [id, count] : onset_sources) {
                std::cout << names[scenario] << " onset_node=" << id << " events=" << count << '\n';
                onset_events += count;
                if ((scenario == 0 && count == 0) || (scenario == 1 && count != 0))
                    throw std::runtime_error("music.onset_event_response");
            }
            std::cout << names[scenario] << " decoded_feature_frames=" << features[scenario].size()
                      << " texture_bytes=" << stable_bytes << " audio_onset_events=" << onset_events
                      << '\n';
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
