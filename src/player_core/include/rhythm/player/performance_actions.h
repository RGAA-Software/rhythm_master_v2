#pragma once

#include <array>

#include "rhythm/parameters/beat_grid.h"
#include "rhythm/runtime/playback_clock.h"

namespace rhythm::player {
enum class PerformanceActionKind { kSnapshot, kNextScene };
enum class PerformanceActionState { kIdle, kPending, kDispatched, kCompleted, kCancelled, kFailed };
enum class PerformanceActionReason {
    kNone,
    kUser,
    kSourceChanged,
    kGridChanged,
    kNoGrid,
    kNoBoundary,
    kTargetUnavailable
};
struct PerformanceAction {
    std::uint64_t id_ = 0;
    PerformanceActionKind kind_ = PerformanceActionKind::kSnapshot;
    std::uint64_t target_ = 0;
    parameters::Quantization quantization_ = parameters::Quantization::kImmediate;
    std::optional<double> due_seconds_{};
    PerformanceActionState state_ = PerformanceActionState::kIdle;
    PerformanceActionReason reason_ = PerformanceActionReason::kNone;
    std::uint64_t replaced_id_ = 0;
    bool operator==(const PerformanceAction&) const = default;
};
// Host-thread request lifecycle, with exactly one slot per action kind. No clock,
// worker, callback, graph mutation or resource ownership. Observe the authoritative
// presentation sample before Request/TakeDue. A changed document/source generation,
// backward time or grid cancels outstanding work. Paused/suspended samples never
// dispatch. Resolve immediately after performing the returned action on this thread.
class PerformanceActions final {
   public:
    void Observe(const runtime::PlaybackSample& sample, std::uint64_t document_generation,
                 const std::optional<parameters::BeatSettings>& grid, bool suspended = false);
    // Repeating a still-pending identical target/mode returns the same ID and
    // retains its original boundary; a different request replaces that slot.
    std::uint64_t Request(PerformanceActionKind kind, std::uint64_t target,
                          parameters::Quantization quantization);
    std::optional<PerformanceAction> TakeDue(PerformanceActionKind kind);
    bool Resolve(std::uint64_t id, bool succeeded);
    bool Cancel(std::uint64_t id);
    void CancelAll(PerformanceActionReason reason = PerformanceActionReason::kUser);
    const PerformanceAction& Status(PerformanceActionKind kind) const;

   private:
    std::array<PerformanceAction, 2> actions_{{{}, {0, PerformanceActionKind::kNextScene}}};
    std::optional<runtime::PlaybackSample> sample_{};
    std::optional<parameters::BeatSettings> grid_{};
    std::uint64_t document_generation_ = 0;
    std::uint64_t next_id_ = 1;
    bool suspended_ = false;
};
}  // namespace rhythm::player
