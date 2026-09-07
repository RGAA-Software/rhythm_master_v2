#include "rhythm/audio/capture.h"

#include <chrono>
#include <condition_variable>
#include <limits>
#include <mutex>
#include <thread>

#include "rhythm/audio/analyzer.h"
#include "wasapi_device.h"

namespace rhythm::audio {
class SystemCapture::Impl final {
   public:
    ~Impl() { Stop(); }
    void Stop() {
        if (worker_.joinable()) {
            worker_.request_stop();
            worker_.join();
        }
        std::lock_guard lock(mutex_);
        snapshot_ = {};
    }
    void Run(std::stop_token stop) {
        try {
            detail::WasapiDevice device;
            Analyzer analyzer;
            analyzer.Reset(device.SampleRate(), ++generation_);
            std::uint64_t position = 0;
            std::uint64_t discontinuities = 0;
            std::condition_variable_any wake;
            std::mutex wait_mutex;
            auto last_packet = std::chrono::steady_clock::now();
            while (!stop.stop_requested()) {
                auto packet = device.Read();
                if (packet) {
                    last_packet = std::chrono::steady_clock::now();
                    if (packet->discontinuity_) {
                        ++discontinuities;
                        analyzer.Reset(device.SampleRate(), ++generation_, position);
                    }
                    if (packet->frames_) {
                        if (!analyzer.Push(std::span(packet->samples_).first(packet->frames_ * 2),
                                           2, position))
                            throw std::runtime_error("audio.invalid_pcm");
                        position += packet->frames_;
                    }
                }
                {
                    std::lock_guard lock(mutex_);
                    snapshot_.state_ = CaptureState::kRunning;
                    snapshot_.features_ = analyzer.Snapshot();
                    snapshot_.discontinuities_ = discontinuities;
                    // A render endpoint can stop producing packets during silence.
                    // Do not leave the last loud frame driving the graph indefinitely.
                    if (std::chrono::steady_clock::now() - last_packet >
                        std::chrono::milliseconds(250))
                        snapshot_.features_ = {};
                }
                if (!packet) {
                    std::unique_lock lock(wait_mutex);
                    wake.wait_for(lock, stop, std::chrono::milliseconds(5), [] { return false; });
                }
            }
        } catch (...) {
            std::lock_guard lock(mutex_);
            snapshot_ = {CaptureState::kFailed};
        }
    }
    mutable std::mutex mutex_{};
    CaptureSnapshot snapshot_{};
    std::uint64_t generation_ = 0;
    std::jthread worker_{};
};
SystemCapture::SystemCapture() : impl_(std::make_unique<Impl>()) {}
SystemCapture::~SystemCapture() = default;
void SystemCapture::Start() {
    impl_->Stop();
    {
        std::lock_guard lock(impl_->mutex_);
        impl_->snapshot_.state_ = CaptureState::kStarting;
    }
    try {
        impl_->worker_ = std::jthread([this](std::stop_token stop) { impl_->Run(stop); });
    } catch (...) {
        std::lock_guard lock(impl_->mutex_);
        impl_->snapshot_ = {CaptureState::kFailed};
    }
}
void SystemCapture::Stop() { impl_->Stop(); }
CaptureSnapshot SystemCapture::Snapshot() const {
    std::lock_guard lock(impl_->mutex_);
    return impl_->snapshot_;
}
}  // namespace rhythm::audio
