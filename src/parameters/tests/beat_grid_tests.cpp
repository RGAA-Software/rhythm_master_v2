#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "rhythm/parameters/beat_grid.h"

namespace {
void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
template <class Action>
void Reject(Action action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    throw std::runtime_error("invalid beat input accepted");
}
}  // namespace
int main() {
    using namespace rhythm::parameters;
    try {
        const BeatGrid common;
        Check(common.BeatDuration() == 0.5 && common.BarDuration() == 2, "default 4/4 grid");
        Check(common.NextAfter(0.5, Quantization::kBeat) == 1 &&
                      common.NextAfter(2, Quantization::kBar) == 4 &&
                      common.NextAfter(0.7, Quantization::kImmediate) == 0.7,
              "strictly future beat/bar and immediate commands");
        const BeatGrid compound({120, 6, 8, 0});
        Check(compound.BeatDuration() == 0.25 && compound.BarDuration() == 1.5 &&
                      compound.NextAfter(1.25, Quantization::kBar) == 1.5,
              "BPM counts quarters while 6/8 counts notated eighths");
        const BeatGrid offset({120, 3, 4, 1});
        const auto before = offset.Position(0.25);
        Check(before.beat_ == -2 && before.bar_ == -1 && before.beat_in_bar_ == 1 &&
                      before.phase_ == 0.5 && offset.NextAfter(0, Quantization::kBar) == 1,
              "pre-origin beats use floor division and retain phase");
        for (const double bpm : {20.0, 97.0, 120.0, 600.0}) {
            for (const double origin : {-1e9, -0.3, 0.0, 1e9}) {
                const BeatGrid grid({bpm, 7, 8, origin});
                for (const auto quantization : {Quantization::kBeat, Quantization::kBar}) {
                    const auto boundary = grid.NextAfter(500.01, quantization).value();
                    Check(grid.NextAfter(std::nextafter(boundary, 0.0), quantization) == boundary &&
                                  grid.NextAfter(boundary, quantization).value() > boundary &&
                                  grid.NextAfter(std::nextafter(boundary, 1e9), quantization) ==
                                          grid.NextAfter(boundary, quantization),
                          "one-ULP boundary requests are neither skipped nor executed early");
                }
            }
        }
        Check(!common.NextAfter(kMaximumBeatSeconds, Quantization::kBeat), "time budget end");
        Reject([] { (void)BeatGrid({0, 4, 4, 0}); });
        Reject([] { (void)BeatGrid({120, 0, 4, 0}); });
        Reject([] { (void)BeatGrid({120, 4, 3, 0}); });
        Reject([&] { (void)common.Position(std::numeric_limits<double>::quiet_NaN()); });
        Reject([&] { (void)common.NextAfter(-1, Quantization::kBeat); });
        Reject([&] { (void)common.NextAfter(0, static_cast<Quantization>(99)); });
        TapTempo tapping;
        Check(!tapping.Tap(1) && !tapping.Tap(1.5) && !tapping.Tap(2), "four taps required");
        Check(tapping.Tap(2.5) == 120, "tap tempo from accepted intervals");
        Check(!tapping.Tap(2.51) && tapping.Count() == 4, "accidental fast tap ignored");
        for (int index = 0; index < 20; ++index) (void)tapping.Tap(3 + index * 0.5);
        Check(tapping.Count() == 8 && tapping.Tap(13) == 120, "bounded rolling tap history");
        Check(!tapping.Tap(30) && tapping.Count() == 1, "long break resets the estimate");
        Check(!tapping.Tap(29) && tapping.Count() == 1, "backward source resets taps");
        tapping.Reset();
        for (int index = 0; index < 3; ++index) Check(!tapping.Tap(index * 0.25, 8), "eighth taps");
        Check(tapping.Tap(0.75, 8) == 120, "eighth-note taps return quarter-note BPM");
        std::cout << "beat signatures, origins, exclusive boundaries, budgets and bounded taps "
                     "pass\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
