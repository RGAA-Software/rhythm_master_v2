#pragma once
#include <future>

#include "rhythm/foundation/blocking_executor.h"
#include "rhythm/project/store.h"
namespace rhythm::project {
struct StoreCompletion {
    std::optional<LoadResult> loaded_{};
    std::uint64_t saved_revision_ = 0;
    std::string error_{};
    std::filesystem::path published_path_{};
    bool template_ = false;
};
// One bounded background transaction. Calls are host-thread only. Destruction
// drains the active operation; saving is never abandoned halfway through commit.
class AsyncStore final {
   public:
    ~AsyncStore();
    bool SaveProject(std::filesystem::path path, editor::Snapshot snapshot);
    bool LoadProject(std::filesystem::path path);
    bool LoadTemplate(std::filesystem::path directory, std::filesystem::path asset_directory);
    bool PublishProject(std::filesystem::path path, editor::Snapshot snapshot,
                        std::filesystem::path asset_directory = {});
    bool Busy() const { return pending_.valid(); }
    std::optional<StoreCompletion> Take();

   private:
    bool Submit(std::function<StoreCompletion()> operation);
    foundation::BlockingExecutor executor_{{1, 1}};
    std::future<StoreCompletion> pending_{};
};
}  // namespace rhythm::project
