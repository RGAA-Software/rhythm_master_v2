#pragma once

#include <future>
#include <stop_token>

#include "rhythm/foundation/blocking_executor.h"
#include "rhythm/player/prepared_package.h"

namespace rhythm::player {
enum class PackageLoadError { kNone, kRead, kInvalid, kInstall, kCancelled };
struct PackageLoadResult {
    std::optional<PreparedPackage> package_{};
    PackageLoadError error_ = PackageLoadError::kNone;
};
// Host-thread requests and polling; one bounded worker owns file I/O, validation
// and optional atomic installation. No worker touches Session/render/UI state.
// Cancellation is cooperative at read chunks and before install; an atomic
// install already started completes and reports success. Sources are not removed.
class PackageLoader final {
   public:
    ~PackageLoader();
    bool StartFile(std::filesystem::path source, std::optional<std::filesystem::path> install = {});
    // Holds a checked immutable file lease across preparation. No platform path
    // is required when consuming an application-managed work.
    bool StartBytes(storage::FileBytes source);
    bool Busy() const { return pending_.valid(); }
    void Cancel();
    std::optional<PackageLoadResult> Take();

   private:
    foundation::BlockingExecutor executor_{{1, 1}};
    std::future<PackageLoadResult> pending_{};
    std::stop_source cancellation_{};
};
}  // namespace rhythm::player
