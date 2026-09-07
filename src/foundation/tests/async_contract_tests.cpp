#include <atomic>
#include <future>
#include <iostream>
#include <source_location>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "rhythm/foundation/blocking_executor.h"

namespace {
using namespace std::chrono_literals;
using namespace rhythm::foundation;
void Check(bool value, std::source_location location = std::source_location::current()) {
    if (!value) throw std::runtime_error("async.contract:" + std::to_string(location.line()));
}
void CheckLimit() {
    std::vector<std::unique_ptr<BlockingExecutor>> pools;
    for (int index = 0; index < 8; ++index)
        pools.push_back(std::make_unique<BlockingExecutor>(BlockingOptions{1, 2}));
    bool rejected = false;
    try {
        BlockingExecutor extra;
    } catch (const std::length_error&) {
        rejected = true;
    }
    Check(rejected);
}
void CheckDrain() {
    std::promise<void> started;
    std::promise<void> release;
    const auto gate = release.get_future().share();
    std::atomic_uint ran{};
    BlockingExecutor pool({1, 2});
    Check(pool.TryPost([&] {
        started.set_value();
        if (gate.wait_for(5s) != std::future_status::ready)
            throw std::runtime_error("async.gate_timeout");
        ++ran;
    }) == SubmitResult::kAccepted);
    Check(started.get_future().wait_for(2s) == std::future_status::ready);
    Check(pool.TryPost([&] { ++ran; }) == SubmitResult::kAccepted);
    Check(pool.TryPost([&] { ++ran; }) == SubmitResult::kAccepted);
    Check(pool.TryPost([] {}) == SubmitResult::kQueueFull);
    Check(pool.TryPost({}) == SubmitResult::kInvalidTask);
    pool.RequestStop(ShutdownMode::kDrain);
    Check(pool.TryPost([] {}) == SubmitResult::kStopped);
    release.set_value();
    pool.Join();
    const auto statistics = pool.Statistics();
    Check(ran == 3 && statistics.completed_ == 3 && statistics.cancelled_ == 0 &&
          statistics.rejected_ == 3 && statistics.queue_high_watermark_ == 2 &&
          pool.WaitForIdle(0ms));
}
struct OnDestroy {
    std::function<void()> callback_{};
    ~OnDestroy() {
        if (callback_) callback_();
    }
};
void CheckCancellation() {
    std::promise<void> started;
    std::promise<void> release;
    const auto gate = release.get_future().share();
    auto pool = std::make_shared<BlockingExecutor>(BlockingOptions{1, 4});
    pool->TryPost([&] {
        started.set_value();
        gate.wait_for(5s);
    });
    Check(started.get_future().wait_for(2s) == std::future_status::ready);
    bool released = false;
    auto marker = std::make_shared<OnDestroy>();
    marker->callback_ = [weak = std::weak_ptr(pool), &released] {
        if (const auto owner = weak.lock()) {
            owner->Statistics();
            released = true;
        }
    };
    pool->TryPost([marker] {});
    marker.reset();
    pool->RequestStop(ShutdownMode::kCancelPending);
    Check(released && pool->Statistics().cancelled_ == 1 && !pool->WaitForIdle(0ms));
    release.set_value();
    pool->Join();
    Check(pool->Statistics().completed_ == 1);
}
void CheckConcurrency() {
    BlockingExecutor pool({2, 64});
    std::atomic_uint accepted{};
    std::atomic_uint executed{};
    std::vector<std::jthread> producers;
    for (int producer = 0; producer < 4; ++producer)
        producers.emplace_back([&] {
            for (int index = 0; index < 256; ++index)
                if (pool.TryPost([&] { ++executed; }) == SubmitResult::kAccepted) ++accepted;
        });
    producers.clear();
    Check(pool.WaitForIdle(5s));
    Check(pool.TryPost([] { throw std::runtime_error("expected task failure"); }) ==
          SubmitResult::kAccepted);
    pool.RequestStop();
    pool.Join();
    Check(executed == accepted && accepted > 0 && pool.Statistics().failed_ == 1 &&
          pool.Statistics().queue_high_watermark_ <= 64);
}
void CheckWorkerRelease() {
    auto started = std::make_shared<std::promise<void>>();
    auto started_future = started->get_future();
    std::promise<void> release;
    const auto gate = release.get_future().share();
    auto pool = std::make_shared<BlockingExecutor>(BlockingOptions{1, 2});
    const std::weak_ptr observer(pool);
    pool->TryPost([pool, started, gate] {
        Check(pool->IsWorkerThread() && !pool->WaitForIdle(0ms));
        pool->RequestStop(ShutdownMode::kDrain);
        started->set_value();
        gate.wait_for(5s);
    });
    Check(started_future.wait_for(2s) == std::future_status::ready);
    pool.reset();
    release.set_value();
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (!observer.expired() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::yield();
    Check(observer.expired());
}
}  // namespace
int main() {
    try {
        CheckLimit();
        CheckDrain();
        CheckCancellation();
        CheckConcurrency();
        CheckWorkerRelease();
        std::cout << "async contracts passed: bounded workers/queue, drain, cancel, reentrant "
                     "cleanup, worker release\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
