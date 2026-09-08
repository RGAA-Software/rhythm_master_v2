#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "rhythm/parameters/controls.h"

namespace {
void Require(bool condition) {
    if (!condition) throw std::runtime_error("control.contract");
}
template <typename Callback>
void Reject(Callback callback) {
    bool rejected = false;
    try {
        callback();
    } catch (const std::exception&) {
        rejected = true;
    }
    Require(rejected);
}
}  // namespace
int main() {
    using namespace rhythm::parameters;
    try {
        const ControlBank bank({{7, "Opening", 0, 1, 0.2}, {12, "Twist", -90, 90, 10}},
                               {{1, "Quiet", {{7, 0}, {12, -30}}},
                                {2, "Bright", {{7, 1}, {12, 60}}},
                                {3, "Earlier snapshot", {{7, 0.5}}}});
        Require(bank.Resolve() == ControlValues{{7, 0.2}, {12, 10}});
        Require(bank.Snapshot(3) == ControlValues{{7, 0.5}, {12, 10}});
        const auto first = bank.Snapshot(1), second = bank.Snapshot(2);
        Require(bank.Blend(first, second, 0) == first && bank.Blend(first, second, 1) == second);
        Require(bank.Blend(first, second, 0.25) == ControlValues{{7, 0.25}, {12, -7.5}});
        for (int i = 0; i < 100; ++i) Require(bank.Blend(first, second, 0.75).at(12) == 37.5);
        Reject([&] { (void)bank.Resolve({{8, 0.5}}); });
        Reject([&] { (void)bank.Resolve({{7, 2}}); });
        Reject([&] { (void)bank.Resolve({{7, std::numeric_limits<double>::quiet_NaN()}}); });
        Reject([&] { (void)bank.Blend(first, second, -0.1); });
        Reject([&] { (void)bank.Snapshot(99); });
        Reject([] { (void)ControlBank({{1, "One"}, {1, "Duplicate"}}); });
        Reject([] { (void)ControlBank({{1, "Bad", 1, 0, 0}}); });
        Reject([] { (void)ControlBank({{1, "Good"}}, {{1, "Foreign", {{9, 0}}}}); });
        std::vector<ControlDefinition> large;
        for (std::uint64_t i = 1; i <= 65; ++i) large.push_back({i, "Control"});
        Reject([&] { (void)ControlBank(large); });
        large.pop_back();
        Require(ControlBank(large).Resolve().size() == 64);
        Require(bank.Resolve().at(7) == 0.2);
        std::cout << "Controls: immutable defaults, partial snapshots, deterministic blends and "
                     "limits passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
