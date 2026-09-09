#include <cmath>
#include <iostream>
#include <stdexcept>

#include "rhythm/player/scene_deck.h"

namespace {
using namespace rhythm;
using namespace rhythm::player;
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
std::string Package(const std::string& title, graph::NodeId id, graph::Canvas canvas = {32, 32}) {
    graph::Registry registry;
    graph::Document document;
    document.id_ = title;
    document.canvas_ = canvas;
    document.nodes_ = {registry.MakeNode(id, "texture.gradient"),
                       registry.MakeNode(id + 1, "output.texture")};
    document.edges_ = {{1, id, id + 1, "source"}};
    document.output_ = id + 1;
    return project::EncodePackage(document, title);
}
void Run() {
    auto renderer = render::Renderer::CreateNull();
    SceneDeck deck;
    const auto first = Package("First", 1);
    const auto second = Package("Second", 11, {32, 64});
    const auto third = Package("Third", 21);
    deck.LoadPrepared(PreparedPackage(first));
    const auto tick = [&](double host, const std::optional<runtime::PlaybackSample>& playback = {},
                          const std::optional<SceneAudioSample>& audio = {}) {
        renderer.BeginFrame();
        const auto frame =
                deck.Tick(host, false, RenderQuality::kOriginal, renderer, {}, playback, {}, audio);
        Require(renderer.IsValid(frame.output_.final_), "accepted scene always has valid output");
        renderer.EndFrame();
        return frame;
    };
    tick(0);
    tick(30);
    Require(deck.Current().Seconds() == 30, "old scene already running on local clock");
    Require(deck.StartTransition(PreparedPackage(second), 2, true) && !deck.AudioReady(),
            "CPU stage does not claim GPU readiness");
    const auto id = deck.TransitionId();
    tick(30.01);
    Require(deck.AudioReady() && deck.Progress() == 0 && deck.Current().Title() == "First",
            "valid incoming frame is held behind old output until audio starts");
    const auto old_origin = deck.Current().Seconds();
    SceneAudioSample sample{id, SceneAudioPhase::kWaiting, true};
    tick(30.02, runtime::PlaybackSample{0, 100, true, {}}, sample);
    Require(deck.Current().Seconds() == old_origin,
            "new zero-origin audio source does not rewind old scene");
    sample.paused_ = false;
    sample.previous_ = SceneAudioPosition{0.1, 0};
    tick(30.03, runtime::PlaybackSample{0.1, 100, false, {}}, sample);
    Require(std::abs(deck.Current().Seconds() - old_origin) < 1e-9 && deck.Progress() == 0,
            "queued sound still holds incoming at zero progress");
    sample.phase_ = SceneAudioPhase::kRunning;
    sample.elapsed_seconds_ = 0.5;
    sample.previous_ = SceneAudioPosition{0.6, 0};
    sample.incoming_ = SceneAudioPosition{0.1, 2};
    tick(30.04, runtime::PlaybackSample{0.6, 100, false, {}}, sample);
    Require(deck.Progress() == 0.25 && std::abs(deck.Current().Seconds() - old_origin - 0.5) < 1e-9,
            "visual dissolve follows consumed audio, old origin retained");
    sample.elapsed_seconds_ = 2;
    sample.previous_ = SceneAudioPosition{2.1, 0};
    sample.incoming_ = SceneAudioPosition{0.05, 10};
    tick(30.05, runtime::PlaybackSample{0.05, 101, false, {}}, sample);
    Require(deck.Transitioning() && deck.Progress() == 1 && deck.Current().Title() == "First",
            "envelope endpoint alone cannot commit an unconfirmed source");
    sample.phase_ = SceneAudioPhase::kCommitted;
    sample.incoming_ = SceneAudioPosition{0.1, 10};
    const auto switched = tick(30.06, runtime::PlaybackSample{0.1, 101, false, {}}, sample);
    Require(switched.switched_ && switched.audio_synchronized_ && switched.transition_id_ == id &&
                    switched.entry_seconds_ == 0.1 && deck.Current().Title() == "Second" &&
                    deck.Current().Seconds() == 0.1 && !deck.AudioPendingId(),
            "committed incoming uses actual loop-local time");
    tick(30.07, runtime::PlaybackSample{0.2, 101, false, {}});
    Require(deck.CanPrepareNext() && deck.Current().Seconds() == 0.2,
            "following frame retains committed audio origin and releases retired resources");

    Require(deck.StartTransition(PreparedPackage(third), 1, true), "stage cancelable next scene");
    const auto cancel_id = deck.TransitionId();
    tick(30.08, runtime::PlaybackSample{0.3, 101, false, {}});
    sample = {cancel_id, SceneAudioPhase::kRunning,  false,
              0.1,       SceneAudioPosition{0.4, 0}, SceneAudioPosition{0.1, 0},
              {}};
    tick(30.09, runtime::PlaybackSample{0.4, 101, false, {}}, sample);
    deck.CancelTransition();
    Require(!deck.Transitioning() && deck.AudioPendingId() == cancel_id && !deck.CanPrepareNext(),
            "visual cancellation waits for bounded audio recovery before accepting a third scene");
    sample.phase_ = SceneAudioPhase::kRecovering;
    sample.previous_ = SceneAudioPosition{0.5, 0};
    tick(30.10, runtime::PlaybackSample{0.2, 102, false, {}}, sample);
    Require(deck.Current().Title() == "Second" && std::abs(deck.Current().Seconds() - 0.5) < 1e-9,
            "old visual stays on its own audio clock during queued new-source recovery");
    sample.phase_ = SceneAudioPhase::kCanceled;
    sample.previous_ = SceneAudioPosition{0.6, 0};
    tick(30.11, runtime::PlaybackSample{0.6, 103, false, {}}, sample);
    Require(deck.CanPrepareNext() && !deck.AudioPendingId(),
            "recovery completes the audio binding");
    tick(30.12, runtime::PlaybackSample{0.7, 103, false, {}});
    Require(std::abs(deck.Current().Seconds() - 0.7) < 1e-9,
            "post-recovery origin remains continuous");

    Require(deck.StartTransition(PreparedPackage(third), 1, true), "stage failed audio source");
    const auto failure_id = deck.TransitionId();
    sample = {failure_id, SceneAudioPhase::kFailed, false, 0, SceneAudioPosition{0.8, 0},
              {},         "audio.bad_asset"};
    tick(30.13, runtime::PlaybackSample{0.8, 103, false, {}}, sample);
    Require(deck.Current().Title() == "Second" && deck.Error() == SceneTransitionError::kAudio &&
                    deck.ErrorDetail() == "audio.bad_asset" && deck.CanPrepareNext(),
            "audio failure preserves accepted scene and reason");
    {
        auto pressure = renderer.CreateTexture({8192, 8128});
        Require(deck.StartTransition(PreparedPackage(Package("Too large", 31, {1280, 720})), 1,
                                     true),
                "CPU package fits before GPU admission");
        const auto failed_gpu_id = deck.TransitionId();
        tick(30.14, runtime::PlaybackSample{0.9, 103, false, {}});
        Require(!deck.Transitioning() && !deck.AudioReady() &&
                        deck.Error() == SceneTransitionError::kBudget,
                "failed GPU frame never requests audio start");
        sample = {failed_gpu_id, SceneAudioPhase::kCanceled};
        tick(30.15, runtime::PlaybackSample{1, 103, false, {}}, sample);
        Require(deck.CanPrepareNext() && deck.Error() == SceneTransitionError::kBudget,
                "canceling unstarted audio retains actual GPU rejection reason");
    }
    Require(deck.StartTransition(PreparedPackage(third), 0, true), "zero-duration audio stage");
    tick(30.16, runtime::PlaybackSample{1.1, 103, false, {}}, sample);
    Require(deck.Transitioning() && deck.Progress() == 0,
            "stale audio sample cannot commit new scene");
    deck.Seek(2);
    Require(!deck.AudioPendingId() && deck.CanPrepareNext(),
            "explicit seek invalidates pending audio binding");
    deck.ReleaseGraphics();
    Require(renderer.Stats().texture_bytes_ == 0, "all scene graphics released");
}
}  // namespace
int main() {
    try {
        Run();
        std::cout << "scene audio integration: GPU readiness, consumed progress, independent "
                     "clocks, rollback and failure passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
