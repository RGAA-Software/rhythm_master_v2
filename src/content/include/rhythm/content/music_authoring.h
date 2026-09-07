#pragma once

#include <filesystem>
#include <future>

#include "rhythm/editor/history.h"
#include "rhythm/foundation/blocking_executor.h"

namespace rhythm::content {
// Removes the binding and its now-unused asset record. Immutable store blobs
// remain available to undo and previous revisions; graph references are retained.
editor::Snapshot UnbindSoundtrack(editor::Snapshot snapshot);
struct MusicImportResult {
    std::string document_{};
    std::uint64_t revision_ = 0;
    std::optional<editor::Snapshot> snapshot_{};
    std::string error_{};
};
// Host-thread controller. One worker copies/hashes/probes immutable music and
// returns an authored value; caller checks document/revision before applying it.
class MusicAuthoring final {
   public:
    ~MusicAuthoring();
    bool Start(editor::Snapshot snapshot, std::filesystem::path assets,
               std::filesystem::path source, float gain, bool loop);
    bool Busy() const { return pending_.valid(); }
    void Cancel();
    std::optional<MusicImportResult> Take();

   private:
    foundation::BlockingExecutor executor_{{1, 1}};
    std::future<MusicImportResult> pending_{};
    std::stop_source cancellation_{};
};
}  // namespace rhythm::content
