#pragma once

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace rhythm::parameters {
// Immutable bounded arithmetic program. Variables are a, b, c and time, supplied
// by the graph. No global clock, I/O, mutation, loops or user-defined calls.
class Expression final {
   public:
    Expression();
    explicit Expression(std::string source);
    [[nodiscard]] std::string_view Source() const { return source_; }
    [[nodiscard]] double Evaluate(const std::array<double, 4>& variables) const;
    bool operator==(const Expression& other) const { return source_ == other.source_; }
    static constexpr std::size_t kMaximumSource = 1024;
    static constexpr std::size_t kMaximumInstructions = 256;

   private:
    enum class Op {
        kLiteral,
        kA,
        kB,
        kC,
        kTime,
        kNegate,
        kAdd,
        kSubtract,
        kMultiply,
        kDivide,
        kModulo,
        kSin,
        kCos,
        kAbs,
        kFloor,
        kCeil,
        kSqrt,
        kMin,
        kMax,
        kClamp,
        kMix
    };
    struct Instruction {
        Op op_ = Op::kLiteral;
        double literal_ = 0;
    };
    class Parser;
    std::string source_{};
    std::vector<Instruction> instructions_{};
};
}  // namespace rhythm::parameters
