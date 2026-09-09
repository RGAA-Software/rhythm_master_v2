#pragma once

#include "rhythm/player/session.h"

namespace rhythm::player {
struct ReplacementFrame {
    runtime::FrameResult output_{};
    bool restored_ = false;
    std::optional<render::Budget> budget_{};
    std::string error_{};
};
// Host-thread owner for an explicit serial GPU replacement. Keep only the last
// accepted output while dropping its simulation/GPU graph. Recovery rebuilds
// fresh history at current media time; it cannot reconstruct discarded history.
class SceneReplacement final {
   public:
    void Begin(Session& current, render::Renderer& renderer, const runtime::FrameResult& output);
    void Recover();
    void Finish();
    void BeginFrame();
    void ReleaseGraphics();
    void PresentationChanged(Session& current);
    bool Active() const { return phase_ != Phase::kIdle && phase_ != Phase::kRetired; }
    bool Restoring() const { return phase_ == Phase::kRecovering || phase_ == Phase::kFailed; }
    ReplacementFrame TickCurrent(Session& current, double monotonic_seconds, render::Extent extent,
                                 render::Renderer& renderer, const runtime::ExternalInputs& inputs,
                                 const runtime::PlaybackSample& playback);
    const runtime::PreparationProgress& Progress() const { return progress_; }

   private:
    enum class Phase { kIdle, kHolding, kRecovering, kFailed, kRetired };
    Phase phase_ = Phase::kIdle;
    render::Texture frozen_{};
    render::Extent frozen_extent_{};
    render::Extent recovery_extent_{};
    runtime::PreparationProgress progress_{};
};
}  // namespace rhythm::player
