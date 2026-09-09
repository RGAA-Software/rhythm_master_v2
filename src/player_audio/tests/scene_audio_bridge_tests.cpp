#include <iostream>
#include <stdexcept>

#include "rhythm/assets/store.h"
#include "rhythm/audio/playback.h"
#include "rhythm/player_audio/scene_audio_bridge.h"

namespace {
using namespace rhythm;
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
std::string Package(const std::string& title, std::span<const project::PackagedAsset> assets = {},
                    const std::optional<media::Soundtrack>& soundtrack = {}) {
    graph::Registry registry;
    graph::Document document;
    document.id_ = title;
    document.canvas_ = {32, 32};
    document.nodes_ = {registry.MakeNode(1, "texture.gradient"),
                       registry.MakeNode(2, "output.texture")};
    document.edges_ = {{1, 1, 2, "source"}};
    document.output_ = 2;
    return project::EncodePackage(document, title, assets, soundtrack);
}
void Run(const std::filesystem::path& directory) {
    assets::Store store(directory / "scene-audio-bridge-assets");
    const auto record = store.Import(directory / "tone.flac", "audio/flac");
    const std::array assets{project::PackagedAsset{record, store.Read(record)}};
    const media::Soundtrack soundtrack{record.id_, "Music", 0.5F, true};
    const auto musical = Package("Musical", assets, soundtrack);
    player::SceneDeck deck;
    deck.EnableAudioTransitions(true);
    deck.LoadPrepared(player::PreparedPackage(Package("Previous")));
    auto renderer = render::Renderer::CreateNull();
    player_audio::SceneAudioBridge bridge;
    audio::PlaybackSnapshot snapshot;
    std::uint64_t next_audio_id = 100;
    unsigned begins = 0;
    unsigned cancels = 0;
    bool reject = false;
    const auto begin = [&](const media::SoundtrackSource& source, double duration, bool paused) {
        ++begins;
        Require(source.binding_ == soundtrack && duration == 1 && !paused,
                "host receives authored source and current transport intent");
        if (reject) throw std::runtime_error("audio.test_rejected");
        snapshot.transition_ = {};
        snapshot.transition_.id_ = next_audio_id++;
        snapshot.transition_.state_ = audio::AudioTransitionState::kPreparing;
        snapshot.state_ = audio::PlaybackState::kPlaying;
        return snapshot.transition_.id_;
    };
    const auto cancel = [&](std::uint64_t id) {
        ++cancels;
        Require(id == snapshot.transition_.id_, "cancel addresses bound audio identity");
        return true;
    };
    const auto poll = [&] { return bridge.Poll(deck, snapshot, begin, cancel); };
    double host = 0;
    const auto tick = [&](const std::optional<player::SceneAudioSample>& sample = {}) {
        renderer.BeginFrame();
        const auto result = deck.Tick(host += 0.01, false, player::RenderQuality::kOriginal,
                                      renderer, {}, {}, {}, sample);
        Require(renderer.IsValid(result.output_.final_), "accepted output remains valid");
        renderer.EndFrame();
        return result;
    };
    tick();
    Require(!poll(), "idle host does not start audio");
    Require(deck.StartTransition(player::PreparedPackage(musical), 1), "stage musical scene");
    const auto scene_id = deck.TransitionId();
    Require(poll()->phase_ == player::SceneAudioPhase::kWaiting && begins == 0,
            "audio cannot start before incoming render succeeds");
    tick(poll());
    Require(deck.AudioReady(), "first incoming output ready");
    Require(poll()->phase_ == player::SceneAudioPhase::kWaiting && begins == 1,
            "ready scene enqueues one request");
    for (int i = 0; i < 4; ++i) tick(poll());
    Require(begins == 1 && deck.Progress() == 0, "preparing never repeats load or advances fade");
    snapshot.transition_.state_ = audio::AudioTransitionState::kMixing;
    snapshot.transition_.elapsed_frames_ = 24000;
    snapshot.transition_.previous_presented_ = true;
    snapshot.transition_.previous_seconds_ = 12.5;
    snapshot.transition_.incoming_presented_ = true;
    snapshot.transition_.incoming_seconds_ = 0.5;
    snapshot.transition_.incoming_iteration_ = 2;
    const auto running = poll();
    Require(running->transition_id_ == scene_id && running->incoming_->iteration_ == 2 &&
                    running->previous_->seconds_ == 12.5 && running->elapsed_seconds_ == 0.5,
            "audio identity and both consumed clocks map to scene identity");
    tick(running);
    Require(deck.Progress() == 0.5, "visual progress follows sampled consumed frames");
    snapshot.transition_.elapsed_frames_ = 48000;
    tick(poll());
    Require(deck.Transitioning(), "endpoint alone cannot promote");
    snapshot.transition_.state_ = audio::AudioTransitionState::kCompleted;
    const auto committed = tick(poll());
    Require(committed.switched_ && committed.audio_synchronized_ &&
                    deck.Current().Title() == "Musical",
            "only confirmed completion promotes the prepared scene");
    tick(poll());

    Require(deck.StartTransition(player::PreparedPackage(musical), 1), "stage cancellation");
    tick(poll());
    (void)poll();
    const auto cancels_before = cancels;
    deck.CancelTransition();
    snapshot.transition_.state_ = audio::AudioTransitionState::kRecovering;
    tick(poll());
    tick(poll());
    Require(cancels == cancels_before + 1 && !deck.CanPrepareNext(),
            "cancel sent once and next scene waits for audio recovery");
    snapshot.transition_.state_ = audio::AudioTransitionState::kCanceled;
    tick(poll());
    Require(deck.CanPrepareNext(), "canceled audio releases readiness gate");
    (void)poll();

    Require(deck.StartTransition(player::PreparedPackage(musical), 1), "stage early cancel");
    const auto begins_before = begins;
    deck.CancelTransition();
    tick(poll());
    Require(begins == begins_before && deck.CanPrepareNext(),
            "cancel before GPU readiness never submits audio");

    reject = true;
    Require(deck.StartTransition(player::PreparedPackage(musical), 1), "stage rejected request");
    tick(poll());
    tick(poll());
    Require(deck.ErrorDetail() == "audio.test_rejected" && deck.Current().Title() == "Musical" &&
                    deck.CanPrepareNext(),
            "host admission failure retains current work and exact reason");
    reject = false;
    Require(deck.StartTransition(player::PreparedPackage(musical), 1), "stage superseded request");
    tick(poll());
    (void)poll();
    ++snapshot.transition_.id_;
    tick(poll());
    Require(deck.ErrorDetail() == "audio.transition_superseded" && deck.CanPrepareNext(),
            "replacement audio identity cannot commit the previous scene request");
    Require(deck.StartTransition(player::PreparedPackage(musical), 1),
            "stage invalid confirmation");
    tick(poll());
    (void)poll();
    snapshot.transition_.state_ = audio::AudioTransitionState::kCompleted;
    snapshot.transition_.incoming_presented_ = false;
    tick(poll());
    Require(deck.ErrorDetail() == "audio.transition_unconfirmed_position",
            "completion without a consumed incoming position is rejected");
    deck.ReleaseGraphics();
}
}  // namespace
int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::invalid_argument("expected media fixture directory");
        Run(argv[1]);
        std::cout << "scene audio bridge: readiness, clocks, commitment, cancellation and failure "
                     "passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
