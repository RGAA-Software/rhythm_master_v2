#pragma once

#include <future>

#include "rhythm/foundation/blocking_executor.h"
#include "rhythm/model_assets/prepare.h"

namespace rhythm::model_assets {
struct LoadRequest {
    graph::ExecutionPlan plan_{};
    std::vector<assets::AssetRecord> assets_{};
    std::filesystem::path directory_{};
    std::uint64_t generation_ = 0;
};
struct LoadResult {
    graph::ExecutionPlan plan_{};
    std::shared_ptr<const scene::Resources> resources_{};
    std::uint64_t generation_ = 0;
    std::string error_{};
};
// Host-thread admission/polling. One active worker and one replaceable latest
// request, using the existing GammaRay foundation executor. Stale completions
// are suppressed; host generation checks also cover edits still compiling.
class Loader final {
   public:
    ~Loader();
    void Submit(LoadRequest request);
    void Cancel();
    bool Busy() const { return pending_.valid() || latest_.has_value(); }
    std::optional<LoadResult> Take();

   private:
    void StartLatest();
    foundation::BlockingExecutor executor_{{1, 1}};
    std::future<LoadResult> pending_{};
    std::optional<LoadRequest> latest_{};
    std::stop_source cancellation_{};
};
}  // namespace rhythm::model_assets
