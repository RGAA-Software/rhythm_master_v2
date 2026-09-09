#include <stdexcept>

#include "rhythm/player/scene_deck.h"

namespace rhythm::player {
void SceneDeck::PrepareQueue(double monotonic_seconds, RenderQuality quality,
                             render::Renderer& renderer, const runtime::ExternalInputs& inputs,
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
    if (!queue || !CanPrepareNext()) return;
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
    preparation_ = incoming_->PrepareGraphics(monotonic_seconds, extent, renderer, incoming_inputs,
                                              {0, scene_generation_, true});
    if (preparation_.state_ == runtime::PreparationState::kPending) return;
    if (preparation_.state_ == runtime::PreparationState::kReady &&
        preparation_.required_passes_ + current_passes_ + 1 > render::kMaximumOffscreenPasses) {
        preparation_.state_ = runtime::PreparationState::kFailed;
        preparation_.budget_ = render::Budget::kPasses;
        preparation_.error_ = "render.pass_budget";
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
