#include <chrono>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>
#include <vector>

#include "rhythm/audio/output.h"

namespace {
void Require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}
void Run() {
    using namespace std::chrono_literals;
    for (int cycle = 0; cycle < 3; ++cycle) {
        rhythm::audio::OutputDevice output;
        Require(output.Snapshot().paused_, "output starts paused");
        output.SetVolume(0);
        const std::vector<float> silence(8192, 0);
        std::uint64_t submitted = 0;
        while (output.Queue(silence)) {
            submitted += 4096;
            Require(submitted <= 24000, "bounded output queue");
        }
        const auto full = output.Snapshot();
        Require(full.queued_frames_ == submitted && full.pulled_frames_ == 0,
                "paused queue conservation");
        Require(!output.Queue(silence) && output.Snapshot().queued_frames_ == submitted,
                "full queue has no mutation");
        bool rejected = false;
        try {
            const std::vector<float> invalid{std::numeric_limits<float>::quiet_NaN(), 0};
            output.Queue(invalid);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        Require(rejected, "invalid PCM rejects");
        output.FinishInput();
        output.Pause(false);
        const auto deadline = std::chrono::steady_clock::now() + 3s;
        while (output.Snapshot().queued_frames_ > 0 &&
               std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(5ms);
        }
        const auto played = output.Snapshot();
        Require(played.pulled_frames_ == submitted && played.queued_frames_ == 0,
                "actual device consumes entire queue");
        output.Pause(true);
        Require(output.Queue(silence), "queue after drain");
        output.Clear();
        const auto cleared = output.Snapshot();
        Require(cleared.queued_frames_ == 0 && cleared.submitted_frames_ == 0 && cleared.paused_,
                "clear resets accounting and preserves pause");
    }
    std::cout << "audio output: three device lifetimes, bounded silent PCM, pause/resume, drain, "
                 "clear and invalid-input rejection passed\n";
}
}  // namespace
int main() {
    try {
        Run();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
