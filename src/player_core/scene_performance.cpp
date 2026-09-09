#include <cmath>
#include <stdexcept>

#include "rhythm/player/scene_deck.h"

namespace rhythm::player {
void SceneDeck::ResetPerformance() {
    actions_.CancelAll(PerformanceActionReason::kSourceChanged);
    live_controls_.clear();
    beat_grid_ = current_->BeatGrid();
    ++performance_generation_;
    actions_.Observe({current_->Seconds(), scene_generation_, current_->Paused()},
                     performance_generation_, beat_grid_);
}
void SceneDeck::SetBeatGrid(std::optional<parameters::BeatSettings> grid) {
    if (grid && !parameters::ValidBeatSettings(*grid))
        throw std::invalid_argument("performance.beat_settings");
    if (grid == beat_grid_) return;
    beat_grid_ = grid;
    actions_.CancelAll(PerformanceActionReason::kGridChanged);
    actions_.Observe({current_->Seconds(), scene_generation_, current_->Paused()},
                     performance_generation_, beat_grid_);
}
void SceneDeck::EditControls(const parameters::ControlValues& changes) {
    (void)current_->Controls().Resolve(changes);
    for (const auto& [id, value] : changes) live_controls_[id] = value;
}
void SceneDeck::FollowCues() {
    live_controls_.clear();
    actions_.Cancel(actions_.Status(PerformanceActionKind::kSnapshot).id_);
}
std::uint64_t SceneDeck::RequestSnapshot(std::uint64_t snapshot, parameters::Quantization mode) {
    (void)current_->Controls().Snapshot(snapshot);
    return actions_.Request(PerformanceActionKind::kSnapshot, snapshot, mode);
}
std::uint64_t SceneDeck::RequestNextScene(std::uint64_t queue_item, double duration,
                                          parameters::Quantization mode) {
    if (!std::isfinite(duration) || duration < 0 || duration > 5)
        throw std::invalid_argument("performance.duration");
    const auto previous = actions_.Status(PerformanceActionKind::kNextScene).id_;
    const auto id = actions_.Request(PerformanceActionKind::kNextScene, queue_item, mode);
    if (previous != id) requested_duration_ = duration;
    return id;
}
void SceneDeck::ApplyPerformance(const runtime::PlaybackSample& sample,
                                 const std::optional<std::reference_wrapper<SceneQueue>>& queue) {
    actions_.Observe(sample, performance_generation_, beat_grid_);
    if (const auto action = actions_.TakeDue(PerformanceActionKind::kSnapshot)) {
        try {
            live_controls_ = current_->Controls().Snapshot(action->target_);
            actions_.Resolve(action->id_, true);
        } catch (const std::invalid_argument&) {
            actions_.Resolve(action->id_, false);
        }
    }
    if (const auto action = actions_.TakeDue(PerformanceActionKind::kNextScene)) {
        if (queue && CanPrepareNext() && QueueReady(action->target_) &&
            !queue->get().Items().empty() && queue->get().Items().front().id_ == action->target_) {
            if (queue->get().StartGraphics(action->target_)) {
                queue_id_ = 0;
                active_queue_id_ = action->target_;
                ActivateTransition(requested_duration_);
                transition_action_ = action->id_;
                return;
            }
        }
        actions_.Resolve(action->id_, false);
    }
}
}  // namespace rhythm::player
