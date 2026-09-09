#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "scene_audio_clock.h"

namespace {
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
}  // namespace
int main() {
    using namespace rhythm;
    using namespace rhythm::player;
    try {
        SceneAudioClock clock;
        clock.Begin(7, 2, 10);
        const runtime::PlaybackSample fallback{30, 10, false, {}};
        auto frame = clock.Step(fallback, {});
        Require(frame.previous_.seconds_ == 30 && frame.incoming_.seconds_ == 0 &&
                        frame.incoming_.paused_ && !frame.committed_,
                "warm-only stage retains old time");
        SceneAudioSample sample{
                7, SceneAudioPhase::kWaiting, false, 0, SceneAudioPosition{0.1, 0}, {}, {}};
        frame = clock.Step(fallback, sample);
        Require(frame.previous_.seconds_ == 30 && clock.PreviousOffset() == 29.9,
                "new silent device clock does not reset an existing visual scene");
        sample.phase_ = SceneAudioPhase::kRunning;
        sample.elapsed_seconds_ = 1;
        sample.previous_ = SceneAudioPosition{1.1, 0};
        sample.incoming_ = SceneAudioPosition{0.2, 4};
        frame = clock.Step({99, 99, false, {}}, sample);
        Require(frame.previous_.seconds_ == 31 && frame.previous_.generation_ == 10 &&
                        frame.incoming_.seconds_ == 0.2 && frame.progress_ == 0.5,
                "elapsed fade is independent of source loops and unrelated primary generation");
        sample.incoming_ = SceneAudioPosition{0.1, 5};
        sample.paused_ = true;
        frame = clock.Step(fallback, sample);
        Require(frame.previous_.generation_ == 10 && frame.incoming_.generation_ == 11 &&
                        frame.incoming_.paused_ && frame.previous_.paused_ &&
                        frame.progress_ == 0.5,
                "loop and pause states remain independent for both scenes");
        sample.phase_ = SceneAudioPhase::kRecovering;
        sample.previous_ = SceneAudioPosition{1.3, 0};
        frame = clock.Step(fallback, sample);
        Require(frame.abort_ && !frame.terminal_ &&
                        std::abs(frame.previous_.seconds_ - 31.2) < 1e-9,
                "queued recovery keeps old visual origin until audible restoration");
        sample.phase_ = SceneAudioPhase::kCanceled;
        Require(clock.Step(fallback, sample).terminal_, "cancel completes after recovery");
        const auto offset = clock.PreviousOffset();
        sample.phase_ = SceneAudioPhase::kRunning;
        sample.incoming_->seconds_ = std::numeric_limits<double>::quiet_NaN();
        bool invalid = false;
        try {
            (void)clock.Step(fallback, sample);
        } catch (const std::invalid_argument&) {
            invalid = true;
        }
        Require(invalid && clock.PreviousOffset() == offset, "invalid snapshot preserves origin");
        clock.Reset();
        clock.Begin(8, 0, 20);
        sample = {7, SceneAudioPhase::kCommitted, false, 0, {}, SceneAudioPosition{0.5, 0}, {}};
        Require(!clock.Step(fallback, sample).committed_,
                "old scene identity cannot commit a new transition");
        sample.transition_id_ = 8;
        frame = clock.Step(fallback, sample);
        Require(frame.committed_ && frame.progress_ == 1 && frame.incoming_.seconds_ == 0.5,
                "zero duration still waits for explicit audio confirmation");
        std::cout << "scene audio values: visual origin, independent loops, pause, recovery and "
                     "late IDs passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
