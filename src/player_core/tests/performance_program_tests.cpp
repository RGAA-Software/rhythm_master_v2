#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

#include "rhythm/player/performance_program.h"
#include "rhythm/player/scene_queue.h"
#include "rhythm/project/package.h"
#include "rhythm/project/performance_store.h"
#include "rhythm/storage/atomic_file.h"

namespace {
void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void Await(rhythm::player::PerformanceProgram& program) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (program.Busy() && std::chrono::steady_clock::now() < deadline) {
        program.Pump();
        std::this_thread::yield();
    }
    Check(!program.Busy(), "performance job timed out");
}
}  // namespace
int main(int argc, char** argv) {
    using namespace rhythm;
    try {
        if (argc != 2) throw std::invalid_argument("test directory required");
        const auto directory =
                std::filesystem::path(argv[1]) /
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        std::filesystem::create_directories(directory);
        graph::Registry registry;
        graph::Document document;
        document.id_ = "performance-job";
        document.nodes_ = {registry.MakeNode(1, "texture.gradient"),
                           registry.MakeNode(2, "output.texture")};
        document.edges_ = {{1, 1, 2, "source"}};
        document.output_ = 2;
        const auto source = directory / "source.rhythmpack";
        storage::WriteDurable(source, project::EncodePackage(document, "Performance job"));
        auto work = player::WorkLibrary(directory / "seed").Import(source).reference_;
        work.source_ = performance::WorkSource::kBuiltin;
        work.content_id_ = "official.templates.job";
        work.version_ = "0.2.0";
        work.policy_ = performance::VersionPolicy::kCurrentBuiltin;
        const auto reader = [source](const performance::WorkReference&, std::stop_token) {
            return storage::FileBytes::Open(source, project::kMaximumFilePackageBytes);
        };
        player::PerformanceProgram program(directory / "program", {work}, reader);
        Check(program.Load() && !program.Save() && !program.Edit(performance::List{}),
              "concurrent job/edit admitted");
        Await(program);
        Check(program.Status().error_.empty() && !program.Status().saved_, "first run failed");
        Check(program.Import(source, 2, parameters::Quantization::kBar), "import start failed");
        Await(program);
        Check(program.Status().error_.empty() && program.Draft().Entries().size() == 1 &&
                      program.Status().dirty_,
              "import not applied on host");
        auto list = program.Draft();
        auto old = work;
        old.version_ = "0.1.0";
        old.package_.sha256_ = std::string(64, 'a');
        Check(list.Append({0, old, "Updated builtin", 1, parameters::Quantization::kBeat})
                      .has_value(),
              "builtin append failed");
        old.content_id_ = "missing.builtin";
        Check(list.Append({0, old, "Missing builtin"}).has_value(), "missing append failed");
        Check(program.Edit(list) && program.Save(), "save start failed");
        Await(program);
        Check(!program.Status().dirty_ && program.Status().saved_, "save status incorrect");
        Check(program.Resolve(), "resolve start failed");
        Await(program);
        auto resolved = program.TakeResolved();
        Check(resolved && resolved->size() == 3 && (*resolved)[0].bytes_.Valid() &&
                      (*resolved)[1].state_ == performance::ResolutionState::kUpdated &&
                      (*resolved)[1].entry_.work_ == work &&
                      (*resolved)[2].state_ == performance::ResolutionState::kMissing &&
                      !(*resolved)[2].bytes_.Valid(),
              "resolution dropped or replaced missing work");
        player::SceneQueue queue;
        Check(queue.ReplacePerformance(*resolved) && queue.Items().size() == 3 &&
                      queue.Items()[2].state_ == player::ScenePreparation::kFailed &&
                      queue.Items()[0].entry_->quantization_ == parameters::Quantization::kBar,
              "queue lost missing row or settings");
        const auto id = queue.Items().front().id_;
        auto invalid = *resolved;
        invalid[1].entry_.id_ = invalid[0].entry_.id_;
        Check(!queue.ReplacePerformance(invalid) && queue.Items().front().id_ == id,
              "invalid replacement changed running queue");
        Check(program.Draft() == list, "resolution consumed or silently edited saved program");
        Check(program.Load(), "reopen start failed");
        Await(program);
        Check(program.Draft() == list, "saved program changed on reopen");
        storage::WriteDurable(directory / "program/list.json", "broken");
        Check(program.Load(), "bad load start failed");
        Await(program);
        Check(!program.Status().error_.empty() && program.Draft() == list,
              "failed load replaced editable program");
        Check(program.Resolve(), "cancel test start failed");
        program.Cancel();
        Await(program);
        Check(program.Status().error_ == "performance.cancelled" && !program.TakeResolved(),
              "cancelled resolution leaked completion");
        // Catalog publication replaces an immutable file while queued readers
        // keep their original lease; in-place mutation is intentionally denied.
        const auto replacement = directory / "replacement.rhythmpack";
        storage::WriteDurable(replacement, project::EncodePackage(document, "Changed bytes"));
        storage::Replace(replacement, source);
        Check(program.Resolve(), "hash check start failed");
        Await(program);
        resolved = program.TakeResolved();
        Check(resolved && !(*resolved)[1].bytes_.Valid() &&
                      (*resolved)[1].error_ == "performance.package_hash",
              "catalog hash not checked");
        std::cout << "performance jobs, persistence, catalog updates, missing rows and "
                     "cancellation passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
