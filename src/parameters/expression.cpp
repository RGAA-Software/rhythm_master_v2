#include "rhythm/parameters/expression.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace rhythm::parameters {
namespace {
double Bounded(double value) { return std::isnan(value) ? 0 : std::clamp(value, -1.0e12, 1.0e12); }
}  // namespace
class Expression::Parser final {
   public:
    explicit Parser(std::string_view source) : source_(source) {}
    std::vector<Instruction> Parse() {
        Sum(0);
        Space();
        if (position_ != source_.size()) throw std::invalid_argument("expression.syntax");
        return std::move(instructions_);
    }

   private:
    void Space() {
        while (position_ < source_.size() &&
               (source_[position_] == ' ' || source_[position_] == '\t' ||
                source_[position_] == '\r' || source_[position_] == '\n'))
            ++position_;
    }
    bool Take(char value) {
        Space();
        if (position_ == source_.size() || source_[position_] != value) return false;
        ++position_;
        return true;
    }
    void Expect(char value) {
        if (!Take(value)) throw std::invalid_argument("expression.syntax");
    }
    void Emit(Op op, double literal = 0) {
        if (instructions_.size() == kMaximumInstructions)
            throw std::length_error("expression.instructions");
        instructions_.push_back({op, literal});
    }
    void Sum(unsigned depth) {
        Product(depth);
        while (true) {
            if (Take('+')) {
                Product(depth);
                Emit(Op::kAdd);
            } else if (Take('-')) {
                Product(depth);
                Emit(Op::kSubtract);
            } else
                break;
        }
    }
    void Product(unsigned depth) {
        Unary(depth);
        while (true) {
            if (Take('*')) {
                Unary(depth);
                Emit(Op::kMultiply);
            } else if (Take('/')) {
                Unary(depth);
                Emit(Op::kDivide);
            } else if (Take('%')) {
                Unary(depth);
                Emit(Op::kModulo);
            } else
                break;
        }
    }
    void Unary(unsigned depth) {
        if (depth > 32) throw std::length_error("expression.depth");
        if (Take('-')) {
            Unary(depth + 1);
            Emit(Op::kNegate);
        } else if (Take('+'))
            Unary(depth + 1);
        else
            Primary(depth);
    }
    void Primary(unsigned depth) {
        if (Take('(')) {
            Sum(depth + 1);
            Expect(')');
            return;
        }
        Space();
        if (position_ == source_.size()) throw std::invalid_argument("expression.syntax");
        const auto start = position_;
        const auto digit = [](char value) { return value >= '0' && value <= '9'; };
        if (digit(source_[position_]) || source_[position_] == '.') {
            double number = 0;
            const auto parsed = std::from_chars(source_.data() + position_,
                                                source_.data() + source_.size(), number);
            if (parsed.ec != std::errc{} || !std::isfinite(number) || std::abs(number) > 1.0e12)
                throw std::invalid_argument("expression.number");
            position_ = static_cast<std::size_t>(parsed.ptr - source_.data());
            Emit(Op::kLiteral, number);
            return;
        }
        while (position_ < source_.size() &&
               ((source_[position_] >= 'a' && source_[position_] <= 'z') ||
                source_[position_] == '_'))
            ++position_;
        if (start == position_) throw std::invalid_argument("expression.syntax");
        const auto name = source_.substr(start, position_ - start);
        if (name == "a")
            Emit(Op::kA);
        else if (name == "b")
            Emit(Op::kB);
        else if (name == "c")
            Emit(Op::kC);
        else if (name == "time")
            Emit(Op::kTime);
        else if (name == "pi")
            Emit(Op::kLiteral, std::numbers::pi);
        else if (name == "tau")
            Emit(Op::kLiteral, 2 * std::numbers::pi);
        else {
            struct Function {
                std::string_view name_{};
                Op op_ = Op::kSin;
                unsigned arguments_ = 1;
            };
            const std::array functions{
                    Function{"sin", Op::kSin},        Function{"cos", Op::kCos},
                    Function{"abs", Op::kAbs},        Function{"floor", Op::kFloor},
                    Function{"ceil", Op::kCeil},      Function{"sqrt", Op::kSqrt},
                    Function{"min", Op::kMin, 2},     Function{"max", Op::kMax, 2},
                    Function{"clamp", Op::kClamp, 3}, Function{"mix", Op::kMix, 3}};
            const auto found =
                    std::find_if(functions.begin(), functions.end(),
                                 [&](const auto& function) { return function.name_ == name; });
            if (found == functions.end()) throw std::invalid_argument("expression.name");
            Expect('(');
            for (unsigned index = 0; index < found->arguments_; ++index) {
                if (index) Expect(',');
                Sum(depth + 1);
            }
            Expect(')');
            Emit(found->op_);
        }
    }
    std::string_view source_{};
    std::size_t position_ = 0;
    std::vector<Instruction> instructions_{};
};
Expression::Expression() : Expression("a") {}
Expression::Expression(std::string source) : source_(std::move(source)) {
    if (source_.empty() || source_.size() > kMaximumSource)
        throw std::length_error("expression.source_size");
    instructions_ = Parser(source_).Parse();
}
double Expression::Evaluate(const std::array<double, 4>& variables) const {
    std::array<double, kMaximumInstructions> stack{};
    std::size_t size = 0;
    const auto pop = [&] { return stack[--size]; };
    for (const auto& instruction : instructions_) {
        double value = 0;
        switch (instruction.op_) {
            case Op::kLiteral:
                value = instruction.literal_;
                break;
            case Op::kA:
                value = variables[0];
                break;
            case Op::kB:
                value = variables[1];
                break;
            case Op::kC:
                value = variables[2];
                break;
            case Op::kTime:
                value = variables[3];
                break;
            case Op::kNegate:
                value = -pop();
                break;
            case Op::kSin:
                value = std::sin(pop());
                break;
            case Op::kCos:
                value = std::cos(pop());
                break;
            case Op::kAbs:
                value = std::abs(pop());
                break;
            case Op::kFloor:
                value = std::floor(pop());
                break;
            case Op::kCeil:
                value = std::ceil(pop());
                break;
            case Op::kSqrt:
                value = std::sqrt(std::max(0.0, pop()));
                break;
            case Op::kClamp: {
                const auto high = pop(), low = pop(), input = pop();
                value = std::clamp(input, std::min(low, high), std::max(low, high));
                break;
            }
            case Op::kMix: {
                const auto amount = std::clamp(pop(), 0.0, 1.0), b = pop(), a = pop();
                value = a + (b - a) * amount;
                break;
            }
            default: {
                const auto b = pop(), a = pop();
                switch (instruction.op_) {
                    case Op::kAdd:
                        value = a + b;
                        break;
                    case Op::kSubtract:
                        value = a - b;
                        break;
                    case Op::kMultiply:
                        value = a * b;
                        break;
                    case Op::kDivide:
                        value = b == 0 ? 0 : a / b;
                        break;
                    case Op::kModulo:
                        value = b == 0 ? 0 : std::fmod(a, b);
                        break;
                    case Op::kMin:
                        value = std::min(a, b);
                        break;
                    case Op::kMax:
                        value = std::max(a, b);
                        break;
                    default:
                        throw std::logic_error("expression.program");
                }
            }
        }
        stack[size++] = Bounded(value);
    }
    return size == 1 ? stack[0] : 0;
}
}  // namespace rhythm::parameters
