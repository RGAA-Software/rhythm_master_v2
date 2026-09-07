#pragma once

#include <memory>
#include <optional>

#include "rhythm/qr/reader.h"

namespace rhythm::qr {
enum class ScanSubmit {
    kAccepted,
    kBusy,
    kRateLimited,
    kInactive,
    kInvalidFrame,
    kInvalidTime,
    kFailed
};
struct CompletedScan {
    std::uint64_t generation_ = 0;
    ScanResult result_{};
};
// Camera-owner thread calls Begin/Submit/Take/Cancel. Exactly one decode/future
// at a time and at most five accepted images per second; busy frames are dropped.
// Submit copies only logical Y rows (<= 1 MiB); no camera buffer outlives the call.
// Begin requires a strictly increasing generation. Cancel suppresses even an
// already completed result; an in-progress native decode finishes before join.
// now_us uses one host monotonic clock across generations; Begin retains throttling.
class AsyncReader final {
   public:
    AsyncReader();
    ~AsyncReader();
    AsyncReader(const AsyncReader&) = delete;
    AsyncReader& operator=(const AsyncReader&) = delete;
    bool Begin(std::uint64_t generation);
    ScanSubmit Submit(const LuminanceView& frame, std::int64_t now_us);
    std::optional<CompletedScan> Take();
    bool Busy() const;
    void Cancel();

   private:
    class Impl;
    std::unique_ptr<Impl> impl_{};
};
}  // namespace rhythm::qr
