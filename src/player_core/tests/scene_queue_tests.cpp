#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

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
        std::cout << "bounded scene preparation, retry, cancellation and move handoff pass\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
