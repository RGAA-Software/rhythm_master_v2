#include <cmath>
#include <iostream>
#include <stdexcept>

#include "rhythm/parameters/control_sequence.h"

int main() {
    using namespace rhythm::parameters;
    try {
        const auto check = [](bool value) {
            if (!value) throw std::runtime_error("cue.contract");
        };
        const ControlBank bank({{1, "Energy", 0, 1, 0.2}},
                               {{1, "Quiet", {{1, 0}}}, {2, "Peak", {{1, 1}}}});
        const ControlSequence sequence(bank, {{1, "Opening", 1, 1, 0, false},
                                              {2, "Rise", 2, 2, 4, false},
                                              {3, "Break", 4, 1, 2, true}});
        check(sequence.Sample(0).at(1) == 0.2 && sequence.Sample(1).at(1) == 0);
        check(sequence.Sample(3).at(1) == 0.25 && sequence.Sample(4).at(1) == 0.5);
        check(sequence.Sample(4.5).at(1) == 0.421875 && sequence.Sample(6).at(1) == 0);
        check(sequence.Active(0) == std::nullopt && sequence.Active(4) == 3);
        check(sequence.Crossed(0, 4) == std::vector<std::uint64_t>({1, 2, 3}));
        check(sequence.Crossed(4, 4).empty() && sequence.Crossed(6, 0).empty());
        for (double time : {4.5, 0.0, 6.0, 3.0, 4.5})
            check(sequence.Sample(time) ==
                  ControlSequence(bank, {sequence.Cues().begin(), sequence.Cues().end()})
                          .Sample(time));
        const auto invalid = [&](std::vector<ControlCue> cues) {
            bool rejected = false;
            try {
                (void)ControlSequence(bank, std::move(cues));
            } catch (const std::exception&) {
                rejected = true;
            }
            check(rejected);
        };
        invalid({{1, "A", 0, 1}, {2, "B", 0, 2}});
        invalid({{1, "A", 0, 99}});
        invalid({{0, "A", 0, 1}});
        invalid({{1, "A", -1, 1}});
        invalid({{1, "A", 1e9, 1, 1}});
        invalid(std::vector<ControlCue>(257));
        const ControlSequence first(bank, {{1, "Start", 0, 2}});
        check(first.Sample(0).at(1) == 1 && first.Active(0) == 1);
        check(EvaluateControls(bank, first, 0, {{1, 0.45}}).at(1) == 0.45);
        check(EvaluateControls(bank, first, 0).at(1) == 1);
        check(EvaluateControls(bank, {}, 0).at(1) == 0.2);
        std::cout << "Cue sequence: interrupted fades, deterministic seeks, crossings and bounds "
                     "passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
