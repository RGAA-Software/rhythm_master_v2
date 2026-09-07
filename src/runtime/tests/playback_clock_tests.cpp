#include <iostream>
#include <limits>
#include <stdexcept>

#include "rhythm/runtime/playback_clock.h"

namespace {
void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
}  // namespace
int main() {
    using namespace rhythm::runtime;
    try {
        PlaybackClock clock;
        Check(clock.Advance(10) == 0 && clock.Advance(11) == 1, "local monotonic clock");
        PlaybackSample audio{4, 1, false, 16};
        Check(clock.Advance(12, false, audio) == 4, "audio takes over local time");
        const auto initial = clock.Generation();
        Check(clock.Advance(100, false, audio) == 4 && clock.Generation() == initial,
              "stalled audio does not extrapolate wall time");
        audio.seconds_ = 5;
        Check(clock.Advance(101, false, audio) == 5, "audio drives animation");
        audio.paused_ = true;
        clock.Advance(102, false, audio);
        Check(clock.Paused() && clock.Advance(103, false, audio) == 5, "audio pause");
        audio.seconds_ = 2;
        ++audio.generation_;
        Check(clock.Advance(104, false, audio) == 2 && clock.Generation() == initial + 1,
              "seek resets history exactly once");
        clock.Advance(105, false, audio);
        Check(clock.Generation() == initial + 1, "paused seek remains stable");
        audio.seconds_ = 0;
        ++audio.generation_;
        audio.paused_ = false;
        clock.Advance(106, false, audio);
        Check(!clock.Paused() && clock.Generation() == initial + 2, "loop resets history");
        audio.seconds_ = 4;
        Check(clock.Advance(110, true, audio) == 0, "host suspension freezes source");
        Check(clock.Advance(111, false, audio) == 4, "resume follows latest consumed sample");
        Check(clock.Advance(112) == 4 && clock.Advance(113) == 5,
              "disconnect continues locally without catch-up");
        Check(!clock.FollowingMedia(), "disconnected source");
        clock.SetPaused(true);
        Check(clock.Advance(114) == 5, "local pause after disconnect");
        clock.Seek(8);
        Check(clock.Advance(115) == 8, "local paused seek");
        bool rejected = false;
        audio.seconds_ = std::numeric_limits<double>::quiet_NaN();
        try {
            clock.Advance(116, false, audio);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        Check(rejected && clock.Seconds() == 8, "invalid source is atomic");
        std::cout
                << "playback clock: audio master, pause, seek, loop, suspension, fallback passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
