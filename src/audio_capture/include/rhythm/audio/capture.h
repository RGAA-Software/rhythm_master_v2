#pragma once

#include <memory>

#include "rhythm/audio/features.h"

namespace rhythm::audio {
enum class CaptureState { kStopped, kStarting, kRunning, kFailed };
struct CaptureSnapshot {
    CaptureState state_ = CaptureState::kStopped;
    Features features_{};
    std::uint64_t discontinuities_ = 0;
};
// Owner-thread commands and snapshots. Worker owns device and analyzer; it
// publishes values only. No PCM is persisted or sent to another process.
class SystemCapture final {
   public:
    SystemCapture();
    ~SystemCapture();
    SystemCapture(const SystemCapture&) = delete;
    SystemCapture& operator=(const SystemCapture&) = delete;
    void Start();
    void Stop();
    CaptureSnapshot Snapshot() const;

   private:
    class Impl;
    std::unique_ptr<Impl> impl_{};
};
}  // namespace rhythm::audio
