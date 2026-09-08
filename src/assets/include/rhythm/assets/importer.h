#pragma once

#include <functional>
#include <future>
#include <optional>
#include <stop_token>
#include <vector>

#include "rhythm/assets/store.h"
#include "rhythm/foundation/blocking_executor.h"

namespace rhythm::assets {
struct ImportResult {
    std::optional<AssetRecord> asset_{};
    std::string error_{};
    std::vector<AssetCheck> checks_{};
    std::optional<AssetRecord> restored_{};
};
// Host-thread calls; exactly one worker owns blocking import I/O. Destruction
// requests cancellation and joins before releasing the worker's captured values.
class Importer final {
   public:
    ~Importer();
    bool Start(std::filesystem::path directory, std::filesystem::path source,
               std::string media_type, std::uint64_t maximum_bytes = 256 * 1024 * 1024);
    bool StartInspect(std::filesystem::path directory, std::vector<AssetRecord> records);
    bool StartRestore(std::filesystem::path directory, std::filesystem::path source,
                      AssetRecord expected);
    bool Busy() const { return pending_.valid(); }
    void Cancel();
    std::optional<ImportResult> Take();

   private:
    bool StartTask(std::function<ImportResult(std::stop_token)> operation);
    foundation::BlockingExecutor executor_{{1, 1}};
    std::stop_source cancellation_{};
    std::future<ImportResult> pending_{};
};
}  // namespace rhythm::assets
