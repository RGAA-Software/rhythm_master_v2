#include <imgui.h>
#include <imgui_internal.h>

#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <thread>

#include "rhythm/assets/store.h"
#include "rhythm/audio_ui/audio_panel.h"
#include "rhythm/platform/host.h"
#include "rhythm/player/work_library.h"
#include "rhythm/player_audio/scene_audio_bridge.h"
#include "scene_queue_panel.h"

namespace {
using namespace rhythm;
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void Activate(const char* window_name, const char* item) {
    // Borrowed ImGui object stays inside the synchronous test UI boundary.
    const auto* window = ImGui::FindWindowByName(window_name);
    Require(window != nullptr, "expected scene queue window");
    ImGui::ActivateItemByID(ImHashStr(item, 0, window->ID));
}
std::string Package(const std::string& title, graph::Color color,
                    std::span<const project::PackagedAsset> assets, float gain, bool serial = false,
                    bool serial_gpu = false) {
    graph::Registry registry;
    graph::Document document;
    document.id_ = title;
    document.canvas_ = {32, 32};
    document.nodes_ = {registry.MakeNode(1, "texture.gradient"),
                       registry.MakeNode(2, "output.texture")};
    for (const auto key : {"color_a", "color_b"}) document.nodes_[0].properties_[key] = color;
    document.edges_ = {{1, 1, 2, "source"}};
    document.output_ = 2;
    if (serial_gpu) {
        document.nodes_.pop_back();
        document.edges_.clear();
        for (std::uint64_t id = 2; id <= 60; ++id) {
            document.nodes_.push_back(registry.MakeNode(id, "texture.transform"));
            document.nodes_.back().properties_["scale"] = 1.0;
            document.edges_.push_back({id, id - 1, id, "source"});
        }
        document.nodes_.push_back(registry.MakeNode(61, "output.texture"));
        document.edges_.push_back({61, 60, 61, "source"});
        document.output_ = 61;
    }
    media::Soundtrack soundtrack{assets.front().record_.id_, title, gain, true};
    if (serial)
        for (std::uint64_t id = 1; id <= 4; ++id) {
            media::AudioClip clip{id, "Concurrent", soundtrack.asset_};
            clip.timing_ = {0, 1, 0, 1};
            clip.gain_ = 0.2F;
            soundtrack.clips_.push_back(clip);
        }
    return project::EncodePackage(document, title, assets, soundtrack);
}
void Pixels(render::Renderer& renderer, render::Readback& ticket, double progress) {
    for (int attempt = 0; attempt < 16; ++attempt) {
        if (const auto image = ticket.Poll()) {
            const auto amount = std::lround(progress * 255) / 255.0;
            for (std::size_t offset = 0; offset < image->rgba_.size(); offset += 4) {
                Require(std::abs(image->rgba_[offset] - 255 * (1 - amount)) <= 2 &&
                                image->rgba_[offset + 1] <= 2 &&
                                std::abs(image->rgba_[offset + 2] - 255 * amount) <= 2 &&
                                image->rgba_[offset + 3] >= 253,
                        "actual GPU color differs from consumed-audio dissolve progress");
            }
            std::cout << "audio_scene_pixels progress=" << progress
                      << " red=" << int(image->rgba_[0]) << " blue=" << int(image->rgba_[2])
                      << '\n';
            return;
        }
        renderer.BeginFrame();
        renderer.EndFrame();
    }
    throw std::runtime_error("audio scene readback timeout");
}
void Run(const std::filesystem::path& root, const std::filesystem::path& fixture,
         bool serial = false, bool serial_gpu = false) {
    assets::Store store(fixture / "scene-audio-ui-assets");
    const auto record = store.Import(fixture / "tone.flac", "audio/flac");
    const std::array assets{project::PackagedAsset{record, store.Read(record)}};
    const auto incoming_path = fixture / "scene-audio-ui-next.rhythmpack";
    project::InstallPackage(incoming_path, Package("Blue incoming", {0, 0, 1, 1}, assets, 0.25F,
                                                   serial, serial_gpu));
    platform::Host host(true);
    host.Resize({1280, 720});
    ImGui::GetIO().IniFilename = nullptr;
    auto renderer = host.CreateRenderer();
    auto font = host.CreateFontTexture(renderer);
    render::Texture pressure;
    if (serial_gpu) {
        const auto rows =
                (256 * 1024 * 1024 - renderer.Stats().texture_bytes_ - 327680) / (8192 * 4);
        pressure = renderer.CreateTexture({8192, static_cast<std::uint16_t>(rows)});
    }
    audio_ui::AudioPanel audio;
    audio.SetVolume(0);
    player::SceneDeck deck;
    deck.EnableAudioTransitions(true);
    deck.LoadPrepared(player::PreparedPackage(
            Package("Red current", {1, 0, 0, 1}, assets, 0.5F, serial, serial_gpu)));
    audio.LoadSoundtrack(*deck.Current().Soundtrack());
    player_audio::SceneAudioBridge bridge;
    player::SceneQueue queue;
    if (serial) {
        player::WorkLibrary library(fixture / "serial-cut-works");
        const auto imported = library.Import(incoming_path);
        const std::array works{player::ResolvedWork{
                {1, imported.reference_, imported.title_, 0, parameters::Quantization::kImmediate},
                performance::ResolutionState::kExact,
                library.Open(imported.reference_)}};
        Require(queue.ReplacePerformance(works),
                "install explicit zero-duration performance entry");
    }
    player_ui::SceneQueuePanel panel;
    const std::vector<player_ui::SceneChoice> choices{
            {incoming_path, {{"en-US", "Blue incoming"}}}};
    std::ifstream catalog(root / "locales/en-US/studio.json");
    const auto text = nlohmann::json::parse(catalog).get<std::map<std::string, std::string>>();
    const auto start = std::chrono::steady_clock::now();
    bool started = false;
    bool midpoint = false;
    bool committed = false;
    bool heard = false;
    std::uint64_t consumed = 0;
    std::uint64_t epoch = 0;
    for (int frame_index = 0; !committed; ++frame_index) {
        const auto seconds =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        Require(seconds < 12 && host.Poll(), "scene audio UI transition deadline");
        queue.Pump(deck.CanPrepareNext());
        host.BeginUi();
        renderer.BeginFrame();
        if (frame_index == 2) Activate("Audio harness", "###scene.queue");
        if (!serial && frame_index == 5) Activate("###scene.queue", "###scene.enqueue");
        const auto input = audio.Frame();
        heard |= input.features_ && input.features_->rms_ > 0.1F;
        if (!started && frame_index > 3 && heard && !queue.Items().empty() &&
            (serial_gpu ? deck.CanHardCut(queue.Items().front().id_)
                        : deck.QueueReady(queue.Items().front().id_))) {
            Activate("###scene.queue", serial_gpu ? "###scene.gpu_hard_cut" : "###scene.go");
            started = true;
            consumed = input.file_.consumed_frames_;
            epoch = input.file_.source_generation_;
        }
        ImGui::Begin("Audio harness");
        panel.Draw(queue, deck, choices, "en-US", text);
        ImGui::End();
        const auto sample = bridge.Poll(
                deck, input.file_,
                [&](const auto& source, double duration, bool paused) {
                    return audio.BeginSoundtrackTransition(source, duration, paused);
                },
                [&](std::uint64_t id) { return audio.CancelSoundtrackTransition(id); });
        runtime::ExternalInputs external;
        external.audio_ = input.features_;
        const auto frame = deck.Tick(seconds, false, player::RenderQuality::kOriginal, renderer,
                                     external, input.playback_, queue, sample);
        if (serial_gpu && deck.Error() != player::SceneTransitionError::kNone)
            std::cout << "serial_gpu frame=" << frame_index << " requested=" << started
                      << " transition=" << deck.Transitioning() << " error=" << int(deck.Error())
                      << " detail=" << deck.ErrorDetail()
                      << " output_budget=" << frame.output_.budget_.has_value() << '\n';
        Require((deck.Error() == player::SceneTransitionError::kNone ||
                 (serial_gpu && deck.TransitionId() == 0 &&
                  deck.Error() == player::SceneTransitionError::kBudget)) &&
                        !frame.output_.budget_,
                "actual UI audio transition retained no hidden failure");
        if (frame.switched_) {
            Require(frame.audio_synchronized_ && input.file_.transition_.state_ ==
                                                         audio::AudioTransitionState::kCompleted,
                    "scene waits for consumed confirmation");
            audio.AdoptSoundtrack(*deck.Current().Soundtrack(), input.file_.transition_.id_);
            Require(audio.Frame().file_.source_generation_ == epoch &&
                            input.file_.consumed_frames_ > consumed && audio.Volume() == 0,
                    "UI handoff preserves device epoch, consumed frames and master volume");
            committed = true;
        }
        const auto progress = committed ? 1 : deck.Progress();
        const bool check_pixels = frame_index == 1 || committed ||
                                  (!midpoint && progress >= 0.35 && progress <= 0.65);
        std::optional<render::Readback> readback;
        if (check_pixels) readback.emplace(renderer.RequestReadback(frame.output_.final_));
        renderer.Submit({}, host.EndUi(), 0x111822ff);
        renderer.EndFrame();
        if (readback) {
            Pixels(renderer, *readback, progress);
            midpoint |= progress > 0 && progress < 1;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    Require(started && (serial || midpoint) && heard && deck.Current().Title() == "Blue incoming" &&
                    queue.Items().empty(),
            "UI enqueue/Go, audible mix and actual pixels all observed");
    deck.ReleaseGraphics();
    std::cout << "Scene audio UI: explicit action, actual playback snapshot, D3D pixels and "
                 "confirmed "
                 "takeover passed\n";
    if (serial)
        std::cout << "Four old plus four incoming clips: explicit zero-duration UI Go passed\n";
    if (serial_gpu)
        std::cout << "GPU and audio budgets both exceeded: explicit serial replacement UI passed\n";
}
void Program(const std::filesystem::path& root, const std::filesystem::path& fixture) {
    const auto packages = fixture.parent_path() / "content/packages";
    const std::array names{"luminous_concerto", "scene_particle_echo", "chromatic_loom"};
    player::WorkLibrary library(fixture / "mixed-program-works");
    std::vector<player::ResolvedWork> works;
    std::vector<render::Extent> canvases;
    for (const auto name : names) {
        const auto path = packages / (std::string(name) + ".rhythmpack");
        const auto imported = library.Import(path);
        works.push_back({{works.size() + 1, imported.reference_, imported.title_, 0.5,
                          parameters::Quantization::kImmediate},
                         performance::ResolutionState::kExact,
                         library.Open(imported.reference_)});
        player::Session scene;
        scene.Open(path);
        canvases.push_back(scene.Canvas());
    }
    platform::Host host(true);
    host.Resize({1280, 720});
    ImGui::GetIO().IniFilename = nullptr;
    auto renderer = host.CreateRenderer();
    auto font = host.CreateFontTexture(renderer);
    player::SceneDeck deck;
    deck.Open(packages / "luminous_concerto.rhythmpack");
    deck.EnableAudioTransitions(true);
    audio_ui::AudioPanel audio;
    audio.SetVolume(0);
    audio.LoadSoundtrack(*deck.Current().Soundtrack());
    player_audio::SceneAudioBridge bridge;
    player::SceneQueue queue;
    Require(queue.ReplacePerformance(works), "install one mixed-canvas performance instance");
    player_ui::SceneQueuePanel panel;
    std::ifstream catalog(root / "locales/en-US/studio.json");
    const auto text = nlohmann::json::parse(catalog).get<std::map<std::string, std::string>>();
    const auto started = std::chrono::steady_clock::now();
    std::size_t accepted = 0;
    bool requested = false;
    bool paused = false;
    bool resized_during_preparation = false;
    double resume_at = 0;
    for (unsigned frame_index = 0; accepted < works.size(); ++frame_index) {
        const double seconds =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
        Require(seconds < 20 && host.Poll(), "mixed performance deadline");
        if (paused && seconds >= resume_at) {
            deck.SetPaused(false);
            audio.ApplyPlayback({false, std::nullopt});
            paused = false;
        }
        queue.Pump(deck.CanPrepareNext());
        auto input = audio.Frame();
        if (!resized_during_preparation && deck.PreparingGraphics() &&
            deck.GraphicsPreparation().completed_nodes_ > 0) {
            host.Resize({1000, 650});
            resized_during_preparation = true;
        }
        host.BeginUi();
        renderer.BeginFrame();
        if (frame_index == 2) Activate("Program harness", "###scene.queue");
        if (!requested && !paused && frame_index > 4 && input.features_ &&
            input.features_->rms_ > 0 && deck.QueueReady(queue.Items().front().id_)) {
            Activate("###scene.queue", "###scene.go");
            requested = true;
        }
        ImGui::Begin("Program harness");
        panel.Draw(queue, deck, {}, "en-US", text);
        ImGui::End();
        const auto sample = bridge.Poll(
                deck, input.file_,
                [&](const auto& source, double duration, bool hold) {
                    return audio.BeginSoundtrackTransition(source, duration, hold);
                },
                [&](std::uint64_t id) { return audio.CancelSoundtrackTransition(id); });
        runtime::ExternalInputs inputs;
        inputs.audio_ = input.features_;
        const auto frame = deck.Tick(seconds, false, player::RenderQuality::kBalanced, renderer,
                                     inputs, input.playback_, queue, sample);
        Require(deck.Error() == player::SceneTransitionError::kNone && !frame.output_.budget_,
                "mixed program keeps exact accepted graph without fallback");
        std::optional<render::Readback> ticket;
        if (frame.switched_) {
            Require(deck.Current().Title() == works[accepted].entry_.title_ &&
                            deck.Current().Canvas() == canvases[accepted] &&
                            queue.Items().size() == works.size() - accepted - 1,
                    "mixed program title, canvas and remaining row identity");
            Require(frame.audio_synchronized_ == (accepted != 1),
                    "visual-only work preserves existing music");
            if (const auto track = deck.Current().Soundtrack())
                audio.AdoptSoundtrack(*track, input.file_.transition_.id_);
            ticket.emplace(renderer.RequestReadback(frame.output_.final_));
            ++accepted;
            requested = false;
        }
        renderer.Submit({}, host.EndUi(), 0x111822ff);
        renderer.EndFrame();
        if (ticket) {
            bool captured = false;
            for (unsigned attempt = 0; attempt < 16; ++attempt) {
                if (const auto image = ticket->Poll()) {
                    std::uint8_t low = 255, high = 0;
                    for (std::size_t offset = 0; offset < image->rgba_.size(); offset += 4)
                        for (unsigned channel = 0; channel < 3; ++channel) {
                            low = std::min(low, image->rgba_[offset + channel]);
                            high = std::max(high, image->rgba_[offset + channel]);
                        }
                    Require(high - low > 16, "authored work rendered nonuniform pixels");
                    std::cout << "program accepted=" << accepted
                              << " canvas=" << deck.Current().Canvas().width_ << 'x'
                              << deck.Current().Canvas().height_ << " rgb_range=" << int(low) << ':'
                              << int(high) << '\n';
                    captured = true;
                    break;
                }
                renderer.BeginFrame();
                renderer.EndFrame();
            }
            Require(captured, "mixed program actual pixel readback");
            if (accepted == 2) {
                deck.SetPaused(true);
                audio.ApplyPlayback({true, 0.25});
                deck.Seek(0.25);
                deck.ReleaseGraphics();
                paused = true;
                resume_at = seconds + 0.3;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    Require(queue.Items().empty() && resized_during_preparation,
            "mixed performance completed all rows after a real pending-preparation resize");
    deck.ReleaseGraphics();
    std::cout << "Mixed program UI: three built-ins, two soundtracks, portrait canvas, "
                 "pause/seek/graphics recovery passed\n";
}
void Arrangement(const std::filesystem::path& package) {
    player::Session work;
    work.Open(package);
    const auto soundtrack = work.Soundtrack();
    Require(soundtrack && soundtrack->binding_.clips_.size() == 4,
            "current four-clip authored arrangement fixture");
    audio_ui::AudioPanel audio;
    audio.SetVolume(0);
    audio.LoadSoundtrack(*soundtrack);
    audio.ApplyPlayback({false, 9});
    const auto wait = [&](const auto& predicate) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (std::chrono::steady_clock::now() < deadline) {
            const auto frame = audio.Frame();
            Require(frame.file_.state_ != audio::PlaybackState::kFailed &&
                            frame.file_.transition_.state_ != audio::AudioTransitionState::kFailed,
                    "authored arrangement transition rejected or playback failed");
            if (predicate(frame)) return frame;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        throw std::runtime_error("authored arrangement transition timeout");
    };
    wait([](const auto& frame) {
        return frame.features_ && frame.features_->rms_ > 0 && frame.playback_->seconds_ > 9.1;
    });
    const auto id = audio.BeginSoundtrackTransition(*soundtrack, 1, false);
    const auto completed = wait([&](const auto& frame) {
        return frame.file_.transition_.id_ == id &&
               frame.file_.transition_.state_ == audio::AudioTransitionState::kCompleted;
    });
    audio.AdoptSoundtrack(*soundtrack, id);
    Require(completed.file_.transition_.previous_seconds_ > 10 &&
                    completed.file_.transition_.incoming_seconds_ > 1 &&
                    completed.file_.transition_.incoming_seconds_ < 2 && audio.Volume() == 0,
            "three-clip old section overlaps one-clip introduction on one device");
    std::cout << "Authored Concerto: old three-clip section to incoming introduction confirmed, "
                 "future peaks do not falsely reject the transition\n";
}
}  // namespace
int main(int argc, char** argv) {
    try {
        if (argc != 3 && !(argc == 4 && (std::string_view(argv[3]) == "--serial" ||
                                         std::string_view(argv[3]) == "--serial-gpu" ||
                                         std::string_view(argv[3]) == "--program")))
            throw std::invalid_argument("expected root, media fixture and optional --serial");
        if (std::string_view(argv[1]) == "--arrangement")
            Arrangement(argv[2]);
        else if (argc == 4 && std::string_view(argv[3]) == "--program")
            Program(argv[1], argv[2]);
        else
            Run(argv[1], argv[2], argc == 4,
                argc == 4 && std::string_view(argv[3]) == "--serial-gpu");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
