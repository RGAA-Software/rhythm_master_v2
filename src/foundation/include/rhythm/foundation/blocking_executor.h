#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>

namespace rhythm::foundation {
struct BlockingOptions {
    std::size_t thread_count_ = 2;
    std::size_t max_pending_tasks_ = 256;
};
enum class SubmitResult { kAccepted, kInvalidTask, kQueueFull, kStopped };
enum class ShutdownMode { kDrain, kCancelPending };
struct BlockingStatistics {
    std::uint64_t submitted_ = 0;
    std::uint64_t completed_ = 0;
    std::uint64_t failed_ = 0;
    std::uint64_t rejected_ = 0;
    std::uint64_t cancelled_ = 0;
    std::uint64_t active_ = 0;
    std::uint64_t pending_ = 0;
    std::uint64_t queue_high_watermark_ = 0;
    std::chrono::nanoseconds total_queue_delay_{};
    std::chrono::nanoseconds total_execution_time_{};
};
// Adapted first-party GammaRay foundation; see provenance/gammaray_common.json.
// Thread-safe admission and bounded queues. Tasks own their captures; callers
// must budget captured payloads separately. Cancelling removes pending tasks;
// active tasks finish cooperatively. Destruction from a task defers native joins
// to a bounded join service, without detached threads or self-join deadlocks.
// Limits: 8 live/draining pools, 16 workers per pool, 65,536 pending tasks per pool.
class BlockingExecutor final {
   public:
    explicit BlockingExecutor(BlockingOptions options = {});
    ~BlockingExecutor();
    BlockingExecutor(const BlockingExecutor&) = delete;
    BlockingExecutor& operator=(const BlockingExecutor&) = delete;
    SubmitResult TryPost(std::function<void()> task);
    void RequestStop(ShutdownMode mode = ShutdownMode::kDrain);
    // The lifecycle owner calls Join after RequestStop. Concurrent Join calls
    // are not a completion barrier; a worker call schedules deferred joining.
    void Join();
    bool WaitForIdle(std::chrono::milliseconds timeout) const;
    bool IsWorkerThread() const;
    BlockingStatistics Statistics() const;

   private:
    class State;
    void Start();
    std::shared_ptr<State> state_{};
};
}  // namespace rhythm::foundation
