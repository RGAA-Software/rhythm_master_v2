#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "rhythm/runtime/motion_phase.h"

namespace {
void Near(double actual, double expected) {
    if (!std::isfinite(actual) || std::abs(actual - expected) > 1e-8)
        throw std::runtime_error("motion_phase.mismatch");
}
double Replay(int steps) {
    rhythm::runtime::MotionPhase phase;
    double result = 0;
    // Identical timestamped changes, observed at both 30 and 120 FPS.
    for (int frame = 0; frame <= steps * 12; ++frame) {
        const double time = double(frame) / steps;
        result = phase.Advance(time, time < 4 ? 1 : (time < 8 ? 2 : 0.5), 16, true);
    }
    return result;
}
}  // namespace
int main() {
    try {
        rhythm::runtime::MotionPhase phase;
        Near(phase.Advance(0, 1, 16, true), 0);
        Near(phase.Advance(5, 2, 16, true), 5);
        Near(phase.Advance(5, 0.5, 16, true), 5);
        Near(phase.Advance(7, 0, 16, true), 6);
        Near(phase.Advance(9, -1, 16, true), 6);
        Near(phase.Advance(10, -1, 16, true), 5);
        Near(phase.Advance(100, 2, 16, false), 5);
        Near(phase.Advance(100, 2, 16, true), 5);
        Near(phase.Advance(101, 2, 16, true), 7);
        Near(phase.Advance(2, 2, 16, false), 4);
        Near(phase.Advance(2, 0.25, 16, true), 4);
        Near(phase.Advance(4, 0.25, 16, true), 4.5);
        Near(Replay(30), 14);
        Near(Replay(120), 14);
        rhythm::runtime::MotionPhase exact_cycle;
        for (int frame = 0; frame <= 960; ++frame) {
            const auto seconds = double(frame) / 30;
            Near(exact_cycle.Advance(seconds, 1, 16, true), std::fmod(seconds, 16));
        }
        rhythm::runtime::MotionPhase cycle;
        Near(cycle.Advance(0, 2, 16, true), 0);
        Near(cycle.Advance(7.5, 2, 16, true), 15);
        Near(cycle.Advance(8, -2, 16, true), 0);
        Near(cycle.Advance(8.5, -2, 16, true), 15);
        bool rejected = false;
        try {
            phase.Advance(5, std::numeric_limits<double>::quiet_NaN(), 16, true);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        if (!rejected) throw std::runtime_error("motion_phase.invalid_rate");
        Near(phase.Advance(6, 0.25, 16, true), 5);
        rhythm::runtime::MotionPhase long_run;
        const auto bounded = long_run.Advance(1e300, 1e6, 16, true);
        if (!std::isfinite(bounded) || bounded < 0 || bounded >= 16)
            throw std::runtime_error("motion_phase.unbounded");
        std::cout << "motion phase: rate, pause, seek, reverse, replay and bounds passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
