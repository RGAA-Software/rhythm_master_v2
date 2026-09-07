#pragma once

#include <memory>
#include <thread>
#include <vector>

namespace rhythm::foundation::detail {
// One pool lease remains alive through deferred joins; this bounds both native
// workers and join batches, including worker-owned executor destruction.
class PoolLease final {
   public:
    PoolLease();
    ~PoolLease();
    PoolLease(const PoolLease&) = delete;
    PoolLease& operator=(const PoolLease&) = delete;

   private:
    class Budget;
    std::shared_ptr<Budget> budget_{};
};
void PrepareJoiner();
void DeferJoin(std::vector<std::thread> threads, std::shared_ptr<void> lifetime);
}  // namespace rhythm::foundation::detail
