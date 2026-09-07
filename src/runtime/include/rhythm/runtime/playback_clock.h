#pragma once

#include <cstdint>
#include <optional>

#include "rhythm/runtime/frame_clock.h"

namespace rhythm::runtime {
// Host-provided presentation time. The host owns decoding/device latency and
// increments generation on load, seek, loop or stop. No wall-time extrapolation
// is applied to a supplied sample: stalled audio must also stall animation.
struct PlaybackSample {
    double seconds_ = 0;
    std::uint64_t generation_ = 0;
    bool paused_ = false;
    std::optional<double> duration_{};
};
struct PlaybackCommand {
    std::optional<bool> paused_{};
    std::optional<double> seek_{};
};

// Host-thread confined transport. Local time is used without a media source.
// Generation identifies discontinuities so consumers reset temporal history;
// seeking a stateful graph starts new history, it does not reconstruct the past.
class PlaybackClock final {
   public:
    double Advance(double host_seconds, bool suspended = false,
                   const std::optional<PlaybackSample>& source = {});
    void SetPaused(bool paused) { paused_ = paused; }
    void Seek(double seconds);
    [[nodiscard]] double Seconds() const { return clock_.Seconds(); }
    [[nodiscard]] bool Paused() const { return paused_; }
    [[nodiscard]] std::uint64_t Generation() const { return generation_; }
    [[nodiscard]] bool FollowingMedia() const { return source_generation_.has_value(); }

   private:
    FrameClock clock_{};
    std::optional<std::uint64_t> source_generation_{};
    std::uint64_t generation_ = 0;
    bool paused_ = false;
};
}  // namespace rhythm::runtime
