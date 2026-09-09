#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

#include "rhythm/player/scene_queue.h"
#include "rhythm/player/session.h"
#include "rhythm/player/work_library.h"
#include "rhythm/storage/atomic_file.h"

namespace {
void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
template <typename Callback>
void Reject(Callback callback) {
    bool rejected = false;
    try {
        callback();
    } catch (const std::exception&) {
        rejected = true;
    }
    Check(rejected, "invalid managed work accepted");
}
}  // namespace
int main(int argc, char** argv) {
    using namespace rhythm;
    try {
        if (argc != 2) throw std::invalid_argument("test directory required");
        const std::filesystem::path root(argv[1]);
        std::filesystem::create_directories(root);
        graph::Document document;
        document.id_ = "managed-work";
        graph::Registry registry;
        document.nodes_ = {registry.MakeNode(1, "texture.gradient"),
                           registry.MakeNode(2, "output.texture")};
        document.edges_ = {{1, 1, 2, "source"}};
        document.output_ = 2;
        const auto bytes = project::EncodePackage(document, "托管演出");
        const auto source = root / "temporary.rhythmpack";
        storage::WriteDurable(source, bytes);
        player::WorkLibrary library(root / "works");
        const auto imported = library.Import(source);
        Check(imported.reference_.source_ == performance::WorkSource::kManaged &&
                      imported.title_ == "托管演出",
              "managed identity/title lost");
        Check(library.Import(source).reference_ == imported.reference_, "import not deduplicated");
        std::filesystem::remove(source);
        player::WorkLibrary reopened(root / "works");
        const auto stored = reopened.Open(imported.reference_);
        player::SceneQueue queue;
        const auto first = queue.EnqueueBytes(stored, imported.title_);
        const auto second = queue.EnqueueBytes(stored, "Encore");
        Check(first && second && first != second && queue.Items().front().source_.empty(),
              "managed queue depends on temporary source path");
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (queue.Items().front().state_ != player::ScenePreparation::kReady &&
               std::chrono::steady_clock::now() < deadline) {
            queue.Pump(true);
            std::this_thread::yield();
        }
        auto prepared = queue.TakeReady();
        Check(prepared.has_value() && queue.Items().size() == 1, "managed preparation failed");
        player::Session session;
        session.LoadPrepared(std::move(*prepared));
        Check(session.Title() == imported.title_, "reopened managed work cannot prepare");
        queue.Clear();
        auto builtin = imported.reference_;
        builtin.source_ = performance::WorkSource::kBuiltin;
        builtin.content_id_ = "official.templates.test";
        builtin.version_ = "0.1.0";
        builtin.policy_ = performance::VersionPolicy::kCurrentBuiltin;
        storage::WriteDurable(source, bytes);
        Check(library.Import(source, builtin).reference_ == builtin,
              "builtin identity not retained");
        storage::WriteDurable(source, project::EncodePackage(document, "Different"));
        Reject([&] { library.Import(source, builtin); });
        Check(project::ReadPackage(library.Open(builtin)).title_ == imported.title_,
              "mismatched replacement damaged managed copy");
        storage::WriteDurable(source, "invalid package");
        Reject([&] { library.Import(source); });
        std::stop_source cancelled;
        cancelled.request_stop();
        Reject([&] { library.Open(builtin, cancelled.get_token()); });
        Check(!queue.EnqueueBytes({}, "invalid"), "invalid file lease queued");
        std::cout << "managed import, cache removal, reopen, queue preparation and exact repair "
                     "passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
