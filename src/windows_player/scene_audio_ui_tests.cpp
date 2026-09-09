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
                    std::span<const project::PackagedAsset> assets, float gain) {
    graph::Registry registry;
    graph::Document document;
    document.id_ = title;
    document.canvas_ = {32, 32};
    document.nodes_ = {registry.MakeNode(1, "texture.gradient"),
                       registry.MakeNode(2, "output.texture")};
    for (const auto key : {"color_a", "color_b"}) document.nodes_[0].properties_[key] = color;
    document.edges_ = {{1, 1, 2, "source"}};
    document.output_ = 2;
    return project::EncodePackage(document, title, assets,
                                  media::Soundtrack{assets.front().record_.id_, title, gain, true});
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
void Run(const std::filesystem::path& root, const std::filesystem::path& fixture) {
    assets::Store store(fixture / "scene-audio-ui-assets");
    const auto record = store.Import(fixture / "tone.flac", "audio/flac");
    const std::array assets{project::PackagedAsset{record, store.Read(record)}};
    const auto incoming_path = fixture / "scene-audio-ui-next.rhythmpack";
    project::InstallPackage(incoming_path, Package("Blue incoming", {0, 0, 1, 1}, assets, 0.25F));
    platform::Host host(true);
    host.Resize({1280, 720});
    ImGui::GetIO().IniFilename = nullptr;
    auto renderer = host.CreateRenderer();
    auto font = host.CreateFontTexture(renderer);
    audio_ui::AudioPanel audio;
    audio.SetVolume(0);
    player::SceneDeck deck;
    deck.EnableAudioTransitions(true);
    deck.LoadPrepared(player::PreparedPackage(Package("Red current", {1, 0, 0, 1}, assets, 0.5F)));
    audio.LoadSoundtrack(*deck.Current().Soundtrack());
    player_audio::SceneAudioBridge bridge;
    player::SceneQueue queue;
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
        if (frame_index == 5) Activate("###scene.queue", "###scene.enqueue");
        const auto input = audio.Frame();
        heard |= input.features_ && input.features_->rms_ > 0.1F;
        if (!started && heard && !queue.Items().empty() &&
            deck.QueueReady(queue.Items().front().id_)) {
            Activate("###scene.queue", "###scene.go");
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
        Require(deck.Error() == player::SceneTransitionError::kNone && !frame.output_.budget_,
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
    Require(started && midpoint && heard && deck.Current().Title() == "Blue incoming" &&
                    queue.Items().empty(),
            "UI enqueue/Go, audible mix and actual pixels all observed");
    deck.ReleaseGraphics();
    std::cout << "Scene audio UI: enqueue/Go, actual playback snapshot, D3D pixels and continuous "
                 "takeover passed\n";
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
        if (argc != 3) throw std::invalid_argument("expected root and media fixture");
        if (std::string_view(argv[1]) == "--arrangement")
            Arrangement(argv[2]);
        else
            Run(argv[1], argv[2]);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
