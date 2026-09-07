// Adapted from GammaRay px_common/blocking_executor.cpp.
// First-party authorization and hashes: provenance/gammaray_common.json.
#include "rhythm/foundation/blocking_executor.h"

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <set>
#include <stdexcept>
#include <thread>
#include <vector>

#include "thread_joiner.h"

namespace rhythm::foundation {
class BlockingExecutor::State final {
   public:
    struct WorkItem {
        std::function<void()> task_{};
        std::chrono::steady_clock::time_point queued_at_{};
    };
    explicit State(BlockingOptions options) : options_(options) {
        if (!options.thread_count_ || options.thread_count_ > 16 || !options.max_pending_tasks_ ||
            options.max_pending_tasks_ > 65536)
            throw std::invalid_argument("async.options");
    }
    detail::PoolLease lease_{};
    BlockingOptions options_{};
    mutable std::mutex mutex_{};
    mutable std::condition_variable condition_{};
    std::deque<WorkItem> queue_{};
    std::vector<std::thread> threads_{};
    std::set<std::thread::id> worker_ids_{};
    BlockingStatistics statistics_{};
    bool accepting_ = true;
    bool stopping_ = false;
    ShutdownMode mode_ = ShutdownMode::kDrain;
};
BlockingExecutor::BlockingExecutor(BlockingOptions options)
    : state_(std::make_shared<State>(options)) {
    detail::PrepareJoiner();
    try {
        Start();
    } catch (...) {
        RequestStop(ShutdownMode::kCancelPending);
        Join();
        throw;
    }
}
BlockingExecutor::~BlockingExecutor() {
    RequestStop(ShutdownMode::kCancelPending);
    Join();
}
void BlockingExecutor::Start() {
    const auto state = state_;
    std::lock_guard lock(state->mutex_);
    for (std::size_t index = 0; index < state->options_.thread_count_; ++index) {
        state->threads_.emplace_back([state] {
            const auto id = std::this_thread::get_id();
            {
                std::lock_guard worker_lock(state->mutex_);
                state->worker_ids_.insert(id);
            }
            for (;;) {
                State::WorkItem item;
                {
                    std::unique_lock worker_lock(state->mutex_);
                    state->condition_.wait(worker_lock, [&] {
                        return state->stopping_ || !state->queue_.empty();
                    });
                    if (state->stopping_ &&
                        (state->mode_ == ShutdownMode::kCancelPending || state->queue_.empty())) {
                        state->worker_ids_.erase(id);
                        state->condition_.notify_all();
                        return;
                    }
                    item = std::move(state->queue_.front());
                    state->queue_.pop_front();
                    ++state->statistics_.active_;
                    state->statistics_.pending_ = state->queue_.size();
                }
                const auto started = std::chrono::steady_clock::now();
                bool failed = false;
                try {
                    item.task_();
                } catch (...) {
                    failed = true;
                }
                const auto elapsed = std::chrono::steady_clock::now() - started;
                // Destroy user captures without the queue/statistics lock held.
                item.task_ = {};
                {
                    std::lock_guard worker_lock(state->mutex_);
                    --state->statistics_.active_;
                    ++state->statistics_.completed_;
                    state->statistics_.failed_ += failed ? 1 : 0;
                    state->statistics_.total_queue_delay_ += started - item.queued_at_;
                    state->statistics_.total_execution_time_ += elapsed;
                }
                state->condition_.notify_all();
            }
        });
    }
}
SubmitResult BlockingExecutor::TryPost(std::function<void()> task) {
    {
        std::lock_guard lock(state_->mutex_);
        if (!task) {
            ++state_->statistics_.rejected_;
            return SubmitResult::kInvalidTask;
        }
        if (!state_->accepting_) {
            ++state_->statistics_.rejected_;
            return SubmitResult::kStopped;
        }
        if (state_->queue_.size() >= state_->options_.max_pending_tasks_) {
            ++state_->statistics_.rejected_;
            return SubmitResult::kQueueFull;
        }
        state_->queue_.push_back({std::move(task), std::chrono::steady_clock::now()});
        ++state_->statistics_.submitted_;
        state_->statistics_.pending_ = state_->queue_.size();
        state_->statistics_.queue_high_watermark_ =
                std::max(state_->statistics_.queue_high_watermark_, state_->statistics_.pending_);
    }
    state_->condition_.notify_one();
    return SubmitResult::kAccepted;
}
void BlockingExecutor::RequestStop(ShutdownMode mode) {
    std::deque<State::WorkItem> cancelled;
    {
        std::lock_guard lock(state_->mutex_);
        state_->accepting_ = false;
        if (!state_->stopping_ || mode == ShutdownMode::kCancelPending) {
            state_->stopping_ = true;
            state_->mode_ = mode;
        }
        if (state_->mode_ == ShutdownMode::kCancelPending) {
            state_->statistics_.cancelled_ += state_->queue_.size();
            cancelled.swap(state_->queue_);
            state_->statistics_.pending_ = 0;
        }
    }
    state_->condition_.notify_all();
}
void BlockingExecutor::Join() {
    std::vector<std::thread> threads;
    bool worker = false;
    {
        std::lock_guard lock(state_->mutex_);
        worker = state_->worker_ids_.contains(std::this_thread::get_id());
        threads.swap(state_->threads_);
    }
    if (threads.empty()) return;
    if (worker) {
        detail::DeferJoin(std::move(threads), state_);
        return;
    }
    for (auto& thread : threads)
        if (thread.joinable()) thread.join();
}
bool BlockingExecutor::WaitForIdle(std::chrono::milliseconds timeout) const {
    std::unique_lock lock(state_->mutex_);
    if (state_->worker_ids_.contains(std::this_thread::get_id())) return false;
    return state_->condition_.wait_for(
            lock, timeout, [&] { return state_->queue_.empty() && !state_->statistics_.active_; });
}
bool BlockingExecutor::IsWorkerThread() const {
    std::lock_guard lock(state_->mutex_);
    return state_->worker_ids_.contains(std::this_thread::get_id());
}
BlockingStatistics BlockingExecutor::Statistics() const {
    std::lock_guard lock(state_->mutex_);
    return state_->statistics_;
}
}  // namespace rhythm::foundation
