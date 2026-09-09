#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

#include "rhythm/player/scene_deck.h"
#include "scene_replacement.h"

namespace {
using namespace rhythm;
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
std::string Chain(std::uint64_t count, const std::string& title) {
    graph::Registry registry;
    graph::Document document;
    document.id_ = title;
    document.canvas_ = {32, 32};
    document.nodes_ = {registry.MakeNode(1, "texture.gradient")};
    for (std::uint64_t id = 2; id <= count; ++id) {
        document.nodes_.push_back(registry.MakeNode(id, "texture.transform"));
        document.nodes_.back().properties_["scale"] = 1.0;
        document.edges_.push_back({id, id - 1, id, "source"});
    }
    document.nodes_.push_back(registry.MakeNode(count + 1, "output.texture"));
    document.edges_.push_back({count + 1, count, count + 1, "source"});
    document.output_ = count + 1;
    return project::EncodePackage(document, title);
}
void Run() {
    auto renderer = render::Renderer::CreateNull();
    auto pressure = renderer.CreateTexture({8192, 8182});
    const auto baseline = renderer.Stats().texture_bytes_;
    player::Session current;
    current.Load(Chain(60, "Accepted"));
    renderer.BeginFrame();
    const auto accepted =
            current.Tick(0, false, {32, 32}, renderer, {}, runtime::PlaybackSample{6, 7, false});
    renderer.EndFrame();
    Require(renderer.IsValid(accepted.final_), "accepted graph fits initial resource budget");
    player::Session candidate;
    candidate.Load(Chain(60, "Candidate"));
    renderer.BeginFrame();
    const auto rejected = candidate.Tick(0, false, {32, 32}, renderer);
    renderer.EndFrame();
    Require(rejected.budget_.has_value(), "two complete graphs exceed the shared budget");
    candidate.ReleaseGraphics();
    player::SceneReplacement replacement;
    replacement.Begin(current, renderer, accepted);
    Require(renderer.Stats().texture_bytes_ == baseline + 32 * 32 * 4 &&
                    current.Title() == "Accepted" && current.Seconds() == 6,
            "serial replacement retains one output while freeing the old graph");
    renderer.BeginFrame();
    const auto held = replacement.TickCurrent(current, 1, {32, 32}, renderer, {}, {7, 7, false});
    const auto incoming = candidate.Tick(1, false, {32, 32}, renderer);
    renderer.EndFrame();
    Require(renderer.IsValid(held.output_.final_) && held.output_.outputs_.empty() &&
                    !incoming.budget_ && renderer.IsValid(incoming.final_),
            "single retained frame plus candidate fits without exposing stale old node handles");
    candidate.ReleaseGraphics();
    replacement.Recover();
    unsigned steps = 0;
    player::ReplacementFrame restored;
    do {
        renderer.BeginFrame();
        restored = replacement.TickCurrent(current, 2 + steps * 0.01, {32, 32}, renderer, {},
                                           {8 + steps * 0.01, 7, false});
        renderer.EndFrame();
        Require(restored.error_.empty() && renderer.IsValid(restored.output_.final_),
                "old frame stays visible throughout bounded restoration");
        Require(++steps < 30, "restoration converges");
    } while (!restored.restored_);
    Require(steps > 1 && current.Title() == "Accepted" &&
                    current.Seconds() == 8 + (steps - 1) * 0.01,
            "restored current graph aligns with latest media sample and retains identity");
    replacement.BeginFrame();
    Require(!renderer.IsValid(accepted.final_) && renderer.IsValid(restored.output_.final_),
            "old held image retires only after the completed restoration frame");
    current.ReleaseGraphics();
    Require(renderer.Stats().texture_bytes_ == baseline, "replacement leaves no extra resources");
    renderer.BeginFrame();
    const auto second = current.Tick(3, false, {32, 32}, renderer);
    renderer.EndFrame();
    replacement.Begin(current, renderer, second);
    auto obstruction = renderer.CreateTexture({160, 160});
    replacement.Recover();
    for (unsigned step = 0; step < 20; ++step) {
        renderer.BeginFrame();
        restored = replacement.TickCurrent(current, 4, {32, 32}, renderer, {}, {9, 7, true});
        renderer.EndFrame();
        if (!restored.error_.empty()) break;
    }
    Require(restored.budget_.has_value() && renderer.IsValid(restored.output_.final_) &&
                    replacement.Restoring(),
            "failed recovery retains the last image and exact budget diagnosis");
    renderer.BeginFrame();
    const auto failed = replacement.TickCurrent(current, 5, {32, 32}, renderer, {}, {9, 7, true});
    Require(failed.budget_.has_value() && renderer.Stats().passes_ == 0,
            "failed recovery does not allocate and retry every host frame");
    renderer.EndFrame();
    obstruction = {};
    replacement.Recover();
    for (unsigned step = 0; step < 20; ++step) {
        renderer.BeginFrame();
        restored = replacement.TickCurrent(current, 6, {32, 32}, renderer, {}, {10, 7, true});
        renderer.EndFrame();
        if (restored.restored_) break;
    }
    Require(restored.restored_ && restored.error_.empty(), "explicit recovery retry succeeds");
    replacement.BeginFrame();
    current.ReleaseGraphics();
    Require(renderer.Stats().texture_bytes_ == baseline, "failed retry releases every lease");
}
void RunDeck(const std::filesystem::path& root) {
    std::filesystem::create_directories(root);
    const auto path = root / "serial.rhythmpack";
    {
        std::ofstream file(path, std::ios::binary);
        file << Chain(60, "Candidate");
        Require(bool(file), "write candidate package");
    }
    auto renderer = render::Renderer::CreateNull();
    auto pressure = renderer.CreateTexture({8192, 8182});
    player::SceneDeck deck;
    deck.LoadPrepared(player::PreparedPackage(Chain(60, "Accepted")));
    player::SceneQueue queue;
    double seconds = 0;
    const auto tick = [&] {
        queue.Pump(deck.CanPrepareNext());
        renderer.BeginFrame();
        const auto frame = deck.Tick(seconds += 0.01, false, player::RenderQuality::kOriginal,
                                     renderer, {}, {}, queue);
        renderer.EndFrame();
        Require(renderer.IsValid(frame.output_.final_), "deck always retains accepted output");
        return frame;
    };
    const auto id = queue.Enqueue(path, "Candidate").value();
    const auto wait_for_failure = [&] {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (std::chrono::steady_clock::now() < deadline) {
            tick();
            if (queue.Items().front().state_ == player::ScenePreparation::kFailed) return;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        throw std::runtime_error("serial queue preparation deadline");
    };
    wait_for_failure();
    Require(deck.CanHardCut(id) && !deck.PreparingGraphics() && !deck.QueueReady(id),
            "failed dual admission exposes explicit hard cut, not normal Go");
    deck.ReleaseGraphics();
    tick();
    Require(deck.CanHardCut(id) && !deck.QueueReady(id),
            "surface release does not convert failed preparation into normal Go readiness");
    Require(!deck.RequestHardCut(id + 1) && deck.RequestHardCut(id), "stable hard-cut target");
    const auto frozen = tick();
    Require(!frozen.switched_ && frozen.output_.outputs_.empty() &&
                    queue.Items().front().state_ == player::ScenePreparation::kTransitioning,
            "freeze frame retains queue ownership and exposes no released node outputs");
    tick();
    deck.CancelTransition();
    Require(deck.RestoringGraphics() && !deck.CanPrepareNext(),
            "cancel gates queue during recovery");
    for (unsigned step = 0; step < 30 && deck.RestoringGraphics(); ++step) tick();
    Require(!deck.RestoringGraphics() && deck.Current().Title() == "Accepted" &&
                    queue.Items().front().preparation_error_ == "player.transition_cancelled",
            "cancel restores the accepted graph and leaves failed row retryable");
    Require(queue.Retry(), "retry after serial cancellation");
    wait_for_failure();
    Require(deck.RequestHardCut(id), "second explicit serial attempt");
    tick();
    bool switched = false;
    for (unsigned step = 0; step < 30 && !switched; ++step) switched = tick().switched_;
    Require(switched && deck.Current().Title() == "Candidate" && queue.Items().empty(),
            "serial candidate fits and queue row retires only on accepted takeover");
    tick();
    deck.ReleaseGraphics();
    Require(renderer.Stats().live_textures_ == 1, "only unrelated pressure texture remains");
}
}  // namespace
int main(int argc, char* argv[]) {
    try {
        Run();
        Require(argc == 2, "scene_replacement output directory");
        RunDeck(argv[1]);
        std::cout << "Scene replacement: frozen output, shared budget, bounded recovery and media "
                     "alignment passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
