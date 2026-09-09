#include "scene_replacement.h"

#include <stdexcept>

namespace rhythm::player {
void SceneReplacement::Begin(Session& current, render::Renderer& renderer,
                             const runtime::FrameResult& output) {
    if (phase_ != Phase::kIdle || !current.Ready())
        throw std::logic_error("player.replacement_busy");
    auto frozen = renderer.RetainTexture(output.final_);
    current.ReleaseGraphics();
    frozen_ = std::move(frozen);
    frozen_extent_ = output.extent_;
    phase_ = Phase::kHolding;
    progress_ = {};
}
void SceneReplacement::Recover() {
    if (phase_ != Phase::kHolding && phase_ != Phase::kFailed) return;
    phase_ = Phase::kRecovering;
    progress_ = {};
}
void SceneReplacement::Finish() {
    if (phase_ != Phase::kIdle) phase_ = Phase::kRetired;
}
void SceneReplacement::BeginFrame() {
    if (phase_ != Phase::kRetired) return;
    frozen_ = {};
    progress_ = {};
    phase_ = Phase::kIdle;
}
void SceneReplacement::ReleaseGraphics() {
    frozen_ = {};
    if (Restoring()) phase_ = Phase::kRecovering;
    if (phase_ == Phase::kRetired) phase_ = Phase::kIdle;
    progress_ = {};
}
void SceneReplacement::PresentationChanged(Session& current) {
    if (!Restoring()) return;
    current.ReleaseGraphics();
    phase_ = Phase::kRecovering;
    progress_ = {};
    // A presentation reset preserves existing texture allocations. Keep the
    // accepted frozen image; only a device release invalidates that ownership.
}
ReplacementFrame SceneReplacement::TickCurrent(Session& current, double monotonic_seconds,
                                               render::Extent extent, render::Renderer& renderer,
                                               const runtime::ExternalInputs& inputs,
                                               const runtime::PlaybackSample& playback) {
    if (!Active()) throw std::logic_error("player.replacement_inactive");
    ReplacementFrame result;
    // All other old node outputs have expired. Publish only the retained image.
    result.output_.final_ = frozen_.Handle();
    result.output_.extent_ = frozen_extent_;
    if (!Restoring()) return result;
    if (phase_ == Phase::kFailed && extent == recovery_extent_) {
        result.budget_ = progress_.budget_;
        result.error_ = progress_.error_;
        return result;
    }
    recovery_extent_ = extent;
    phase_ = Phase::kRecovering;
    try {
        progress_ = current.PrepareGraphics(monotonic_seconds, extent, renderer, inputs, playback);
        if (progress_.state_ == runtime::PreparationState::kPending) return result;
        if (progress_.budget_) throw render::BudgetExceeded(*progress_.budget_);
        if (progress_.state_ == runtime::PreparationState::kFailed)
            throw std::runtime_error(progress_.error_);
        // Preparation captured an earlier fixed input. Align the restored graph
        // with this frame's authoritative sample before exposing its output.
        auto restored = current.Tick(monotonic_seconds, false, extent, renderer, inputs, playback);
        if (restored.budget_) throw render::BudgetExceeded(*restored.budget_);
        if (!renderer.IsValid(restored.final_))
            throw std::runtime_error("player.replacement_restore_output");
        result.output_ = std::move(restored);
        result.restored_ = true;
        Finish();
        return result;
    } catch (const render::BudgetExceeded& error) {
        progress_.budget_ = error.Kind();
        progress_.error_ = error.what();
    } catch (const std::exception& error) {
        progress_.error_ = error.what();
    }
    current.ReleaseGraphics();
    progress_.state_ = runtime::PreparationState::kFailed;
    progress_.output_.reset();
    phase_ = Phase::kFailed;
    result.budget_ = progress_.budget_;
    result.error_ = progress_.error_;
    return result;
}
}  // namespace rhythm::player
