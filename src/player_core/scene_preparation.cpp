#include <stdexcept>

#include "rhythm/player/scene_deck.h"

namespace rhythm::player {
void SceneDeck::ReportQueueOutcome(const std::optional<std::reference_wrapper<SceneQueue>>& queue) {
    if (!queue_outcome_ || !queue) return;
    queue->get().FinishGraphics(queue_outcome_->id_, queue_outcome_->accepted_,
                                queue_outcome_->error_);
    queue_outcome_.reset();
}
void SceneDeck::PrepareQueue(double monotonic_seconds, RenderQuality quality,
                             render::Renderer& renderer, render::TextureHandle current_output,
                             const runtime::ExternalInputs& inputs,
                             const std::optional<std::reference_wrapper<SceneQueue>>& queue) {
    if (queue && discarded_queue_id_) {
        queue->get().FailGraphics(discarded_queue_id_, "player.preparation_cancelled");
        discarded_queue_id_ = 0;
    }
    if (queue_id_ &&
        (!queue || queue->get().Items().empty() || queue->get().Items().front().id_ != queue_id_)) {
        DiscardTransition();
        discarded_queue_id_ = 0;
    }
    if (!queue || !CanPrepareNext() || !renderer.IsValid(current_output)) return;
    auto& pending = queue->get();
    if (!incoming_ && !pending.Items().empty()) {
        const auto id = pending.Items().front().id_;
        if (auto package = pending.BeginGraphics()) {
            incoming_ = std::make_unique<Session>();
            incoming_->LoadPrepared(std::move(*package));
            queue_id_ = id;
            warmed_ = false;
            preparation_ = {};
            error_ = SceneTransitionError::kNone;
            error_detail_.clear();
        }
    }
    if (!incoming_ || !queue_id_) return;
    const auto extent = PlaybackExtent(incoming_->Canvas(), quality);
    if (warmed_ &&
        (extent != preparation_extent_ || !renderer.IsValid(preparation_.output_->final_))) {
        incoming_->ReleaseGraphics();
        warmed_ = false;
    }
    if (warmed_) return;
    pending.UpdateGraphics(queue_id_, false);
    preparation_extent_ = extent;
    auto incoming_inputs = inputs;
    incoming_inputs.controls_.clear();
    try {
        preparation_ = incoming_->PrepareGraphics(monotonic_seconds, extent, renderer,
                                                  incoming_inputs, {0, scene_generation_, true});
        if (preparation_.state_ == runtime::PreparationState::kPending) return;
        if (preparation_.state_ == runtime::PreparationState::kReady) {
            if (preparation_.required_passes_ + current_passes_ + 1 >
                render::kMaximumOffscreenPasses)
                throw render::BudgetExceeded(render::Budget::kPasses);
            // Reserve the dissolve target/program before publishing queue readiness.
            // The private warm-up result never replaces the accepted output.
            compositor_.Blend(renderer, {current_output, current_->Canvas()},
                              {preparation_.output_->final_, incoming_->Canvas()},
                              PlaybackExtent(current_->Canvas(), quality), 0);
        }
    } catch (const render::BudgetExceeded& error) {
        preparation_.state_ = runtime::PreparationState::kFailed;
        preparation_.budget_ = error.Kind();
        preparation_.error_ = error.what();
    } catch (const std::exception& error) {
        preparation_.state_ = runtime::PreparationState::kFailed;
        preparation_.error_ = error.what();
    }
    if (preparation_.state_ == runtime::PreparationState::kReady) {
        warmed_ = true;
        pending.UpdateGraphics(queue_id_, true);
        return;
    }
    const auto error = preparation_.error_;
    const auto budget = preparation_.budget_;
    pending.FailGraphics(queue_id_, error);
    DiscardTransition();
    discarded_queue_id_ = 0;
    error_ = budget ? SceneTransitionError::kBudget : SceneTransitionError::kRender;
    error_detail_ = error;
}
}  // namespace rhythm::player
