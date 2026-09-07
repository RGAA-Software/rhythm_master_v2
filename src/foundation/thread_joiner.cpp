// Focused adaptation of GammaRay async_runtime.cpp's RuntimeThreadJoiner.
// Source identity and modifications: provenance/gammaray_common.json.
#include "thread_joiner.h"

#include <array>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <stdexcept>

namespace rhythm::foundation::detail {
class PoolLease::Budget final {
   public:
    std::atomic_uint active_{};
};
PoolLease::PoolLease() {
    static const auto budget = std::make_shared<Budget>();
    budget_ = budget;
    auto active = budget_->active_.load();
    do {
        if (active >= 8) throw std::length_error("async.pool_limit");
    } while (!budget_->active_.compare_exchange_weak(active, active + 1));
}
PoolLease::~PoolLease() { --budget_->active_; }
namespace {
struct Batch {
    std::vector<std::thread> threads_{};
    std::shared_ptr<void> lifetime_{};
};
class ThreadJoiner final {
   public:
    ThreadJoiner()
        : worker_([state = state_] {
              for (;;) {
                  Batch batch;
                  {
                      std::unique_lock lock(state->mutex_);
                      state->condition_.wait(lock,
                                             [&] { return state->stopping_ || state->count_; });
                      if (!state->count_ && state->stopping_) return;
                      batch = std::move(state->batches_[state->first_]);
                      state->first_ = (state->first_ + 1) % state->batches_.size();
                      --state->count_;
                  }
                  for (auto& thread : batch.threads_)
                      if (thread.joinable()) thread.join();
              }
          }) {}
    ~ThreadJoiner() {
        {
            std::lock_guard lock(state_->mutex_);
            state_->stopping_ = true;
        }
        state_->condition_.notify_one();
        worker_.join();
    }
    void Submit(Batch batch) {
        {
            std::lock_guard lock(state_->mutex_);
            if (state_->count_ == state_->batches_.size())
                throw std::logic_error("async.join_budget");
            state_->batches_[(state_->first_ + state_->count_) % state_->batches_.size()] =
                    std::move(batch);
            ++state_->count_;
        }
        state_->condition_.notify_one();
    }

   private:
    struct State {
        std::mutex mutex_{};
        std::condition_variable condition_{};
        std::array<Batch, 8> batches_{};
        std::size_t first_ = 0;
        std::size_t count_ = 0;
        bool stopping_ = false;
    };
    std::shared_ptr<State> state_ = std::make_shared<State>();
    std::thread worker_{};
};
ThreadJoiner& Joiner() {
    static ThreadJoiner joiner;
    return joiner;
}
}  // namespace
void PrepareJoiner() { Joiner(); }
void DeferJoin(std::vector<std::thread> threads, std::shared_ptr<void> lifetime) {
    Joiner().Submit({std::move(threads), std::move(lifetime)});
}
}  // namespace rhythm::foundation::detail
