#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

#include "rhythm/audio/capture.h"

int main() {
    using namespace std::chrono_literals;
    try {
        rhythm::audio::SystemCapture capture;
        for (int attempt = 0; attempt < 3; ++attempt) {
            capture.Start();
            const auto deadline = std::chrono::steady_clock::now() + 5s;
            auto state = rhythm::audio::CaptureState::kStarting;
            do {
                std::this_thread::sleep_for(10ms);
                state = capture.Snapshot().state_;
            } while (state == rhythm::audio::CaptureState::kStarting &&
                     std::chrono::steady_clock::now() < deadline);
            if (state != rhythm::audio::CaptureState::kRunning)
                throw std::runtime_error("capture.device_start");
            std::this_thread::sleep_for(100ms);
            if (!rhythm::audio::ValidFeatures(capture.Snapshot().features_))
                throw std::runtime_error("capture.invalid_features");
            capture.Stop();
            capture.Stop();
            if (capture.Snapshot().state_ != rhythm::audio::CaptureState::kStopped)
                throw std::runtime_error("capture.stop");
        }
        capture.Start();  // Destruction during asynchronous device startup.
        std::cout << "WASAPI default output opened/restarted/closed; feature snapshots valid. No "
                     "PCM persisted.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
