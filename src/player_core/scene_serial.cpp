#include <utility>

#include "rhythm/player/scene_deck.h"
#include "scene_replacement.h"

namespace rhythm::player {
bool SceneDeck::CanHardCut(std::uint64_t id) const {
    return id && queue_id_ == id && incoming_ && CanPrepareNext() &&
           preparation_.state_ == runtime::PreparationState::kFailed && preparation_.budget_;
}
bool SceneDeck::RequestHardCut(std::uint64_t id) {
    if (!CanHardCut(id)) return false;
    hard_cut_request_ = id;
    return true;
}
bool SceneDeck::RestoringGraphics() const { return replacement_->Restoring(); }
void SceneDeck::RetryGraphicsRecovery() {
    if (replacement_->Restoring()) replacement_->Recover();
}
bool SceneDeck::PreparingGraphics() const {
    return (replacement_->Restoring() &&
            replacement_->Progress().state_ != runtime::PreparationState::kFailed) ||
           (incoming_ && !warmed_ && !cancel_requested_ &&
            preparation_.state_ != runtime::PreparationState::kFailed);
}
const runtime::PreparationProgress& SceneDeck::GraphicsPreparation() const {
    return replacement_->Restoring() ? replacement_->Progress() : preparation_;
}
bool SceneDeck::BeginHardCut(render::Renderer& renderer, const runtime::FrameResult& output,
                             const std::optional<std::reference_wrapper<SceneQueue>>& queue) {
    const auto id = std::exchange(hard_cut_request_, 0);
    if (!CanHardCut(id) || !queue || queue->get().Items().empty() ||
        queue->get().Items().front().id_ != id ||
        queue->get().Items().front().state_ != ScenePreparation::kFailed ||
        !renderer.IsValid(output.final_))
        return false;
    replacement_->Begin(*current_, renderer, output);
    // Queue mutation and renderer ownership remain on this host thread.
    queue->get().StartReplacement(id);
    active_queue_id_ = id;
    queue_id_ = 0;
    incoming_->ReleaseGraphics();
    compositor_.ReleaseGraphics();
    preparation_ = {};
    warmed_ = false;
    current_passes_ = 0;
    ActivateTransition(0);
    return true;
}
runtime::FrameResult SceneDeck::TickAccepted(double monotonic_seconds, render::Extent extent,
                                             render::Renderer& renderer,
                                             const runtime::ExternalInputs& inputs,
                                             const runtime::PlaybackSample& playback) {
    if (!replacement_->Active())
        return current_->Tick(monotonic_seconds, false, extent, renderer, inputs, playback);
    auto frame = replacement_->TickCurrent(*current_, monotonic_seconds, extent, renderer, inputs,
                                           playback);
    if (!frame.error_.empty()) {
        error_ = frame.budget_ ? SceneTransitionError::kBudget : SceneTransitionError::kRender;
        error_detail_ = frame.error_;
    } else if (frame.restored_) {
        error_ = SceneTransitionError::kNone;
        error_detail_.clear();
    }
    return std::move(frame.output_);
}
}  // namespace rhythm::player
