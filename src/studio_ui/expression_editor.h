#pragma once

#include <array>
#include <map>
#include <optional>
#include <string>

#include "rhythm/parameters/expression.h"

namespace rhythm::studio {
// Retains an invalid edit without replacing the last valid graph expression.
class ExpressionEditor final {
   public:
    std::optional<parameters::Expression> Draw(const parameters::Expression& expression,
                                               const std::string& id,
                                               const std::map<std::string, std::string>& text);
    void Reset() { id_.clear(); }

   private:
    std::string id_{};
    std::string source_{};
    std::array<char, parameters::Expression::kMaximumSource + 1> buffer_{};
    bool invalid_ = false;
};
}  // namespace rhythm::studio
