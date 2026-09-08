#pragma once

#include <future>

#include "rhythm/content/user_components.h"
#include "rhythm/foundation/blocking_executor.h"

namespace rhythm::content {
struct ComponentEntry {
    std::filesystem::path directory_{};
    std::string title_{};
};
struct LibraryResult {
    std::optional<std::vector<ComponentEntry>> entries_{};
    std::optional<editor::Snapshot> component_{};
    std::filesystem::path saved_{};
    std::string expected_document_{};
    std::uint64_t expected_revision_ = 0;
    std::string error_{};
};
// One bounded host-owned worker. Saves drain on destruction; workers return
// values and never mutate editor/library UI state. No disk I/O on insertion.
class ComponentLibrary final {
   public:
    explicit ComponentLibrary(std::filesystem::path directory);
    ~ComponentLibrary();
    bool Refresh();
    bool Save(editor::Snapshot snapshot, graph::NodeId instance,
              std::filesystem::path source_assets);
    bool Load(std::filesystem::path directory, std::filesystem::path destination_assets,
              std::string expected_document, std::uint64_t expected_revision);
    bool LoadOfficial(Semantic semantic, std::filesystem::path destination_assets,
                      std::string expected_document, std::uint64_t expected_revision);
    bool Busy() const { return pending_.valid(); }
    std::optional<LibraryResult> Take();
    const std::filesystem::path& Directory() const { return directory_; }

   private:
    bool Submit(std::function<LibraryResult()> operation);
    std::filesystem::path directory_{};
    foundation::BlockingExecutor executor_{{1, 1}};
    std::future<LibraryResult> pending_{};
};
}  // namespace rhythm::content
