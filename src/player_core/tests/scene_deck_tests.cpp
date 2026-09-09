#include <cmath>
#include <iostream>

#include "rhythm/player/scene_deck.h"

namespace {
void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
std::string Package(const std::string& title, rhythm::graph::NodeId id,
                    rhythm::graph::Canvas canvas = {32, 32}) {
    using namespace rhythm;
    graph::Registry registry;
    graph::Document document;
    document.id_ = title;
    document.canvas_ = canvas;
    document.nodes_ = {registry.MakeNode(id, "control.scalar"),
                       registry.MakeNode(id + 1, "texture.gradient"),
                       registry.MakeNode(id + 2, "output.texture")};
    document.nodes_[0].properties_["value"] = 0.2;
    document.edges_ = {{1, id, id + 1, "amount"}, {2, id + 1, id + 2, "source"}};
    document.control_titles_[id] = "Energy";
    document.control_snapshots_ = {{1, "Quiet", {{id, 0.1}}}, {2, "Bright", {{id, 0.9}}}};
    document.beat_grid_ = parameters::BeatSettings{};
    document.output_ = id + 2;
    return project::EncodePackage(document, title);
}
}  // namespace
int main() {
    using namespace rhythm;
    try {
        auto renderer = render::Renderer::CreateNull();
        const auto first = Package("First", 1);
        const auto second = Package("Second", 11, {32, 64});
        const auto third = Package("Third", 21);
        player::SceneDeck deck;
        deck.LoadPrepared(player::PreparedPackage(first));
        double monotonic = 0;
        runtime::ExternalInputs inputs;
        inputs.controls_ = {{1, 0.8}};
        const auto tick = [&](double seconds, std::uint64_t generation, bool paused = false) {
            renderer.BeginFrame();
            const auto result =
                    deck.Tick(monotonic += 0.25, false, player::RenderQuality::kOriginal, renderer,
                              inputs, runtime::PlaybackSample{seconds, generation, paused, 16});
            Check(renderer.IsValid(result.output_.final_), "valid deck output");
            renderer.EndFrame();
            if (result.switched_) inputs.controls_.clear();
            return result;
        };
        tick(5, 10);
        const auto recall = deck.RequestSnapshot(2, parameters::Quantization::kBeat);
        tick(5.49, 10);
        Check(deck.Current().CurrentControls().at(1) == 0.8, "recall is not early");
        tick(5.5, 10, true);
        Check(deck.Current().CurrentControls().at(1) == 0.8, "paused recall waits");
        tick(5.5, 10);
        Check(deck.Current().CurrentControls().at(1) == 0.9 &&
                      deck.ActionStatus(player::PerformanceActionKind::kSnapshot).id_ == recall &&
                      deck.ActionStatus(player::PerformanceActionKind::kSnapshot).state_ ==
                              player::PerformanceActionState::kCompleted,
              "snapshot reaches current rendered frame at beat");
        deck.EditControls({{1, 0.4}});
        tick(5.5, 10, true);
        Check(deck.Current().CurrentControls().at(1) == 0.4, "manual control after recall");
        deck.FollowCues();
        tick(5.5, 10);
        Check(deck.Current().CurrentControls().at(1) == 0.8, "follow clears performance overrides");
        deck.RequestSnapshot(1, parameters::Quantization::kBar);
        deck.SetBeatGrid(parameters::BeatSettings{97, 6, 8, 0.1});
        Check(deck.ActionStatus(player::PerformanceActionKind::kSnapshot).reason_ ==
                      player::PerformanceActionReason::kGridChanged,
              "tempo edit cancels outstanding recall immediately");
        Check(deck.StartTransition(player::PreparedPackage(second), 2), "stage prepared scene");
        tick(5.5, 10);
        Check(deck.Transitioning() && deck.Progress() == 0 && !deck.CanPrepareNext(),
              "warm one incoming scene");
        tick(6.5, 10);
        Check(deck.Progress() == 0.5 && deck.Current().Title() == "First",
              "midpoint retains active work");
        tick(6.5, 10, true);
        tick(6.5, 10, true);
        Check(deck.Progress() == 0.5 && deck.Current().Paused(),
              "pause freezes both timeline and transition");
        tick(2, 11);
        Check(!deck.Transitioning() &&
                      deck.Error() == player::SceneTransitionError::kDiscontinuity &&
                      deck.Current().Title() == "First",
              "seek cancels transition, retaining active work");
        Check(deck.StartTransition(player::PreparedPackage(second), 0), "stage immediate cut");
        const auto cut = tick(2.2, 11);
        Check(cut.switched_ && cut.entry_seconds_ == 0 && deck.Current().Title() == "Second" &&
                      !deck.CanPrepareNext(),
              "promotion keeps retired owners through presentation");
        tick(2.4, 11);
        Check(deck.CanPrepareNext() && std::abs(deck.Current().Seconds() - 0.2) < 1e-9 &&
                      deck.Current().Controls().Definitions().size() == 1 &&
                      deck.Current().CurrentControls().at(11) == 0.2,
              "scene-local time and controls change while old music continues");
        Check(deck.StartTransition(player::PreparedPackage(third), 1), "stage next scene");
        tick(2.6, 11);
        const auto promoted = tick(3.6, 11);
        Check(promoted.switched_ && std::abs(promoted.entry_seconds_ - 1) < 1e-9,
              "incoming time at promotion");
        deck.AdoptMedia({1, 12, true, 16});
        tick(1, 12, true);
        Check(deck.Current().Seconds() == 1 && deck.Error() == player::SceneTransitionError::kNone,
              "explicit audio handoff preserves scene position during decoder preparation");
        tick(1.2, 12);
        Check(std::abs(deck.Current().Seconds() - 1.2) < 1e-9,
              "new soundtrack resumes at matching scene time");
        bool rejected = false;
        try {
            deck.AdoptMedia({0, 13, false, 16});
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        Check(rejected, "reject mismatched handoff position");
        Check(!deck.StartTransition(player::PreparedPackage(first), -1), "reject invalid duration");
        deck.ReleaseGraphics();
        // The global resource table, not a per-session expanded allowance,
        // enforces admission. Current scene still fits; the incoming 720p scene does not.
        tick(1.3, 12);
        {
            auto pressure = renderer.CreateTexture({8192, 8128});
            Check(deck.StartTransition(player::PreparedPackage(Package("Large", 31, {1280, 720})),
                                       1),
                  "CPU preparation does not allocate scene textures");
            tick(1.4, 12);
            Check(!deck.Transitioning() && deck.Error() == player::SceneTransitionError::kBudget &&
                          deck.Current().Title() == "Third",
                  "GPU rejection retains current scene");
        }
        Check(deck.StartTransition(player::PreparedPackage(first), 0),
              "retry after releasing pressure");
        Check(tick(1.5, 12).switched_, "retry promotes a valid scene");
        tick(1.6, 12);
        deck.ReleaseGraphics();
        Check(renderer.Stats().texture_bytes_ == 0, "deck releases both sessions and compositor");
        std::cout
                << "scene clock, pause, seek, promotion, audio handoff and budget recovery pass\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
