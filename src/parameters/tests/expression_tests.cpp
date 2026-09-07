#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "rhythm/parameters/expression.h"

int main() {
    using rhythm::parameters::Expression;
    try {
        const auto require = [](bool value) {
            if (!value) throw std::runtime_error("expression.contract");
        };
        const auto evaluate = [](const std::string& source) {
            return Expression(source).Evaluate({2, 3, 4, 0.25});
        };
        require(evaluate("a + b * c") == 14);
        require(evaluate("(a+b)*c") == 20);
        require(evaluate("-a--b") == 1);
        require(evaluate("8/2/2") == 2);
        require(evaluate("1e-2 + .5") == 0.51);
        require(std::abs(evaluate("sin(time*tau)") - 1) < 1e-12);
        require(evaluate("max(a,min(b,c))") == 3);
        require(evaluate("clamp(a, 1, 0)") == 1);
        require(evaluate("mix(a,c,0.25)") == 2.5);
        require(evaluate("floor(2.8)+ceil(1.2)+abs(-4)") == 8);
        require(evaluate("sqrt(-1)+a/0+b%0") == 0);
        require(evaluate("1e12*1e12") == 1e12);
        require(Expression("a").Evaluate({std::numeric_limits<double>::quiet_NaN(), 0, 0, 0}) == 0);
        for (const auto source : {"", "a=2", "sin()", "min(1)", "a b", "random()", "a.foo", "1e999",
                                  "[1]", "a; b", "1+", "mix(1,2,3,4)"}) {
            bool rejected = false;
            try {
                Expression expression(source);
            } catch (const std::exception&) {
                rejected = true;
            }
            require(rejected);
        }
        for (const auto& source : {std::string(33, '(') + "a" + std::string(33, ')'),
                                   std::string(1025, 'a'), std::string(33, '-') + "1"}) {
            bool rejected = false;
            try {
                Expression expression(source);
            } catch (const std::exception&) {
                rejected = true;
            }
            require(rejected);
        }
        std::string instructions = "1";
        for (int index = 0; index < 128; ++index) instructions += "+1";
        bool rejected = false;
        try {
            Expression expression(instructions);
        } catch (const std::exception&) {
            rejected = true;
        }
        require(rejected);
        std::cout << "Expressions passed: precedence, variables, pure math, domain handling, "
                     "syntax/depth/size/instruction bounds\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
