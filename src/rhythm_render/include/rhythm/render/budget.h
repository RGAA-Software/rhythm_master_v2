#pragma once

#include <stdexcept>

namespace rhythm::render {
// Admission failures are recoverable by reducing content or retrying after
// releasing resources. Invalid handles and backend faults remain separate errors.
enum class Budget { kTextureBytes, kTextureSlots, kPasses, kBackendResources };

class BudgetExceeded final : public std::length_error {
   public:
    explicit BudgetExceeded(Budget budget)
        : std::length_error("render.resource_budget"), budget_(budget) {}
    Budget Kind() const { return budget_; }

   private:
    Budget budget_ = Budget::kTextureBytes;
};
}  // namespace rhythm::render
