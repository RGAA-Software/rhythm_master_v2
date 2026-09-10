#pragma once

#include <cstdint>
#include <optional>

#include "rhythm/runtime/frame_clock.h"

namespace rhythm::runtime {
// Continuous consumed-source time. Epoch changes on load/seek, never on a
// natural loop. Seconds must be monotonic within an epoch.
struct MotionTime {
    double seconds_ = 0;
    std::uint64_t generation_ = 0;
    bool operator==(const MotionTime&) const = default;
};
// Host-provided presentation time. The host owns decoding/device latency and
// increments generation on load, seek, loop or stop. No wall-time extrapolation
// is applied to a supplied sample: stalled audio must also stall animation.
struct PlaybackSample {
    double seconds_ = 0;
    std::uint64_t generation_ = 0;
    bool paused_ = false;
    std::optional<double> duration_{};
    std::optional<MotionTime> continuous_{};
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
    // Local timeline loop: rebase timeline/effect history while motion continues.
    void Loop(double seconds);
    [[nodiscard]] double Seconds() const { return clock_.Seconds(); }
    [[nodiscard]] bool Paused() const { return paused_; }
    [[nodiscard]] std::uint64_t Generation() const { return generation_; }
    [[nodiscard]] bool FollowingMedia() const { return source_generation_.has_value(); }
    [[nodiscard]] MotionTime Motion() const { return motion_; }

   private:
    FrameClock clock_{};
    std::optional<std::uint64_t> source_generation_{};
    std::uint64_t generation_ = 0;
    bool paused_ = false;
    MotionTime motion_{};
    std::optional<MotionTime> motion_source_{};
};
}  // namespace rhythm::runtime
