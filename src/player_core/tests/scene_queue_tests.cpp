#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

#include "rhythm/player/scene_deck.h"
#include "rhythm/player/scene_queue.h"
#include "rhythm/player/session.h"

namespace {
void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
}  // namespace
int main(int argc, char* argv[]) {
    using namespace rhythm;
    try {
        Check(argc == 2, "scene_queue output");
        const auto root =
                std::filesystem::path(argv[1]) /
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        std::filesystem::create_directories(root);
        graph::Registry registry;
        graph::Document document;
        document.id_ = "queue-scene";
        document.nodes_ = {registry.MakeNode(1, "texture.gradient"),
                           registry.MakeNode(2, "output.texture")};
        document.edges_ = {{1, 1, 2, "source"}};
        document.output_ = 2;
        const auto bytes = project::EncodePackage(document, "Queued scene");
        const auto good = root / "valid.rhythmpack";
        const auto missing = root / "missing.rhythmpack";
        const auto write = [&](const std::filesystem::path& path) {
            std::ofstream file(path, std::ios::binary);
            file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            Check(bool(file), "write test package");
        };
        write(good);
        const auto survivor = root / "survivor.rhythmpack";
        {
            const auto different = project::EncodePackage(document, "Survivor scene");
            std::ofstream file(survivor, std::ios::binary);
            file.write(different.data(), static_cast<std::streamsize>(different.size()));
            Check(bool(file), "write distinct survivor");
        }
        player::SceneQueue queue;
        const auto wait = [&](player::ScenePreparation state) {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            while (std::chrono::steady_clock::now() < deadline) {
                queue.Pump(true);
                if (!queue.Items().empty() && queue.Items().front().state_ == state) return;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            throw std::runtime_error("queue deadline");
        };
        Check(!queue.Enqueue({}, "Empty") && !queue.Enqueue(good, ""), "reject empty selection");
        const auto first = queue.Enqueue(good, "First").value();
        queue.Enqueue(missing, "Missing");
        queue.Pump(false);
        Check(!queue.Busy() && queue.Items().front().state_ == player::ScenePreparation::kQueued,
              "transition gate does not start a third scene");
        wait(player::ScenePreparation::kReady);
        auto prepared = queue.TakeReady();
        Check(prepared && prepared->Ready() && queue.Items().size() == 1 && !queue.Busy(),
              "only one prepared result, no automatic prefetch on transfer");
        player::Session session;
        session.LoadPrepared(std::move(*prepared));
        Check(!prepared->Ready() && session.Title() == "Queued scene", "move-only handoff");
        wait(player::ScenePreparation::kFailed);
        Check(queue.Items().front().error_ == player::PackageLoadError::kRead &&
                      session.Title() == "Queued scene" && !queue.TakeReady(),
              "failure retains current scene");
        write(missing);
        Check(queue.Retry(), "retry failed head");
        wait(player::ScenePreparation::kReady);
        queue.Clear();
        Check(!queue.TakeReady() && queue.Items().empty(), "clear drops prepared scene");
        const auto removed = queue.Enqueue(good, "Removed").value();
        queue.Pump(true);
        Check(queue.Remove(removed) && !queue.Remove(first), "remove preparing row by stable ID");
        const auto next = queue.Enqueue(survivor, "Survivor").value();
        Check(next > removed, "identities never reused");
        wait(player::ScenePreparation::kReady);
        Check(queue.Items().front().id_ == next, "survivor identity");
        auto survivor_package = queue.TakeReady();
        Check(survivor_package.has_value(), "survivor prepared");
        session.LoadPrepared(std::move(*survivor_package));
        Check(session.Title() == "Survivor scene", "late result cannot replace survivor contents");
        for (std::size_t index = 0; index < player::SceneQueue::kMaximumItems; ++index)
            Check(queue.Enqueue(good, "Queued").has_value(), "queue capacity");
        Check(!queue.Enqueue(good, "Overflow"), "queue budget enforced before I/O");
        queue.Pump(true);
        queue.Clear();
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (queue.Busy() && std::chrono::steady_clock::now() < deadline) {
            queue.Pump(false);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        Check(!queue.Busy() && queue.Items().empty() && !queue.TakeReady(),
              "canceled queue drains without resurrection");
        {
            player::SceneDeck deck;
            deck.LoadPrepared(player::PreparedPackage(bytes));
            deck.SetBeatGrid(parameters::BeatSettings{});
            auto renderer = render::Renderer::CreateNull();
            double monotonic = 0;
            const auto tick = [&](double seconds) {
                renderer.BeginFrame();
                const auto frame =
                        deck.Tick(monotonic += 0.1, false, player::RenderQuality::kOriginal,
                                  renderer, {}, runtime::PlaybackSample{seconds, 1, false}, queue);
                Check(renderer.IsValid(frame.output_.final_), "quantized scene output");
                renderer.EndFrame();
                return frame;
            };
            tick(0.1);
            const auto id = queue.Enqueue(survivor, "Next").value();
            wait(player::ScenePreparation::kReady);
            const auto action = deck.RequestNextScene(id, 0, parameters::Quantization::kBeat);
            Check(!tick(0.49).switched_ && queue.Items().front().id_ == id,
                  "pending next scene retains queue ownership");
            Check(deck.QueueReady(id) &&
                          queue.Items().front().state_ == player::ScenePreparation::kPresentable &&
                          !deck.Transitioning(),
                  "GPU preparation precedes Go without consuming the pending row");
            const auto prepared_bytes = renderer.Stats().texture_bytes_;
            Check(tick(0.5).switched_ && queue.Items().empty() &&
                          deck.Current().Title() == "Survivor scene" &&
                          deck.ActionStatus(player::PerformanceActionKind::kNextScene).id_ ==
                                  action &&
                          deck.ActionStatus(player::PerformanceActionKind::kNextScene).state_ ==
                                  player::PerformanceActionState::kCompleted,
                  "next scene displays at the requested beat");
            Check(renderer.Stats().texture_bytes_ == prepared_bytes,
                  "Go reuses prepared scene and compositor allocations");
            tick(0.6);
            deck.SetBeatGrid(parameters::BeatSettings{});
            const auto stale = queue.Enqueue(good, "Removed").value();
            wait(player::ScenePreparation::kReady);
            deck.RequestNextScene(stale, 0, parameters::Quantization::kBeat);
            Check(queue.Remove(stale), "remove pending target");
            const auto survivor_id = queue.Enqueue(good, "Do not consume").value();
            wait(player::ScenePreparation::kReady);
            Check(!tick(1.1).switched_ && queue.Items().front().id_ == survivor_id &&
                          deck.ActionStatus(player::PerformanceActionKind::kNextScene).state_ ==
                                  player::PerformanceActionState::kFailed,
                  "stale target cannot switch to a different queue item");
            Check(deck.QueueReady(survivor_id), "surviving queue head prepares independently");
            queue.Clear();
            tick(1.2);
            Check(deck.CanPrepareNext() && !deck.PreparingGraphics(),
                  "clear discards a GPU prepared candidate on the next host frame");
            const auto failing_id = queue.Enqueue(good, "Budget retry").value();
            wait(player::ScenePreparation::kReady);
            {
                // Leave room for the accepted 640x360 output, but not a second.
                auto pressure = renderer.CreateTexture({8192, 8148});
                tick(1.3);
                Check(queue.Items().front().id_ == failing_id &&
                              queue.Items().front().state_ == player::ScenePreparation::kFailed &&
                              !queue.Items().front().preparation_error_.empty() &&
                              !deck.QueueReady(failing_id) &&
                              deck.Current().Title() == "Survivor scene",
                      "GPU rejection retains failed row, exact diagnostic and old scene");
            }
            Check(queue.Retry(), "GPU failure supports explicit retry");
            wait(player::ScenePreparation::kReady);
            tick(1.4);
            Check(deck.QueueReady(failing_id), "retry becomes presentable after resource release");
            deck.ReleaseGraphics();
            Check(!deck.QueueReady(failing_id), "surface release invalidates queue GPU readiness");
            tick(1.5);
            Check(deck.QueueReady(failing_id), "surface restoration prepares the same stable row");
            const auto warmed_bytes = renderer.Stats().texture_bytes_;
            deck.RequestNextScene(failing_id, 1, parameters::Quantization::kImmediate);
            Check(!tick(1.6).switched_ && queue.Items().empty() && deck.Transitioning() &&
                          renderer.Stats().texture_bytes_ == warmed_bytes,
                  "valid Go consumes retried candidate and reuses its prepared dissolve target");
            Check(tick(2.7).switched_, "retried candidate completes the requested fade");
            deck.ReleaseGraphics();
        }
        std::cout << "bounded scene preparation, retry, cancellation and move handoff pass\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
