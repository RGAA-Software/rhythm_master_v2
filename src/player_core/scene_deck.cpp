#include "rhythm/player/scene_deck.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace rhythm::player {
void SceneDeck::ResetClock() {
    master_ = {};
    master_generation_ = master_.Generation();
    ++scene_generation_;
    origin_ = 0;
    clock_observed_ = false;
    handoff_generation_.reset();
    CancelTransition();
    retired_.reset();
    ResetPerformance();
}
void SceneDeck::Open(const std::filesystem::path& path) {
    auto next = std::make_unique<Session>();
    next->Open(path);
    current_ = std::move(next);
    ResetClock();
}
void SceneDeck::LoadPrepared(PreparedPackage package) {
    auto next = std::make_unique<Session>();
    next->LoadPrepared(std::move(package));
    current_ = std::move(next);
    ResetClock();
}
bool SceneDeck::StartTransition(PreparedPackage package, double duration) {
    if (incoming_ || retired_) {
        error_ = SceneTransitionError::kBusy;
        return false;
    }
    if (!current_->Ready() || !package.Ready() || !std::isfinite(duration) || duration < 0 ||
        duration > 5) {
        error_ = SceneTransitionError::kInvalid;
        return false;
    }
    auto next = std::make_unique<Session>();
    next->LoadPrepared(std::move(package));
    incoming_ = std::move(next);
    duration_ = duration;
    progress_ = 0;
    warmed_ = false;
    error_ = SceneTransitionError::kNone;
    return true;
}
void SceneDeck::SetPaused(bool paused) {
    master_.SetPaused(paused);
    current_->SetPaused(paused);
    if (incoming_) incoming_->SetPaused(paused);
}
void SceneDeck::Seek(double seconds) {
    master_.Seek(seconds);
    origin_ = 0;
    handoff_generation_.reset();
    CancelTransition();
    actions_.CancelAll(PerformanceActionReason::kSourceChanged);
}
void SceneDeck::CancelTransition() {
    if (transition_action_) actions_.Cancel(*transition_action_);
    transition_action_.reset();
    incoming_.reset();
    compositor_.ReleaseGraphics();
    progress_ = 0;
    warmed_ = false;
    error_ = SceneTransitionError::kNone;
}
void SceneDeck::ReleaseGraphics() {
    current_->ReleaseGraphics();
    if (incoming_) incoming_->ReleaseGraphics();
    retired_.reset();
    compositor_.ReleaseGraphics();
}
void SceneDeck::AdoptMedia(const runtime::PlaybackSample& sample) {
    if (incoming_ || !std::isfinite(sample.seconds_) || sample.seconds_ < 0 ||
        (sample.duration_ && (!std::isfinite(*sample.duration_) || *sample.duration_ <= 0)) ||
        std::abs(sample.seconds_ - current_->Seconds()) > 1.0 / 48000)
        throw std::invalid_argument("player.media_handoff_position");
    origin_ = 0;
    handoff_generation_ = sample.generation_;
}
SceneDeckFrame SceneDeck::Tick(double monotonic_seconds, bool suspended, RenderQuality quality,
                               render::Renderer& renderer, const runtime::ExternalInputs& inputs,
                               const std::optional<runtime::PlaybackSample>& playback,
                               const std::optional<std::reference_wrapper<SceneQueue>>& queue) {
    if (!runtime::ValidExternalInputs(inputs))
        throw std::invalid_argument("runtime.external_inputs");
    if (retired_) {
        retired_.reset();
        compositor_.ReleaseGraphics();
    }
    const auto seconds = master_.Advance(monotonic_seconds, suspended, playback);
    if (suspended) return {};
    if (master_generation_ != master_.Generation()) {
        master_generation_ = master_.Generation();
        if (!(handoff_generation_ && playback && *handoff_generation_ == playback->generation_))
            actions_.CancelAll(PerformanceActionReason::kSourceChanged);
        if (clock_observed_ &&
            !(handoff_generation_ && playback && *handoff_generation_ == playback->generation_)) {
            const bool interrupted = Transitioning();
            CancelTransition();
            origin_ = 0;
            ++scene_generation_;
            if (interrupted) error_ = SceneTransitionError::kDiscontinuity;
        }
    }
    clock_observed_ = true;
    handoff_generation_.reset();
    if (!current_->Ready()) return {};
    const runtime::PlaybackSample current_time{
            std::max(0.0, seconds - origin_), scene_generation_, master_.Paused(), {}};
    ApplyPerformance(current_time, queue);
    auto current_inputs = inputs;
    for (const auto& [id, value] : live_controls_) current_inputs.controls_[id] = value;
    const auto extent = PlaybackExtent(current_->Canvas(), quality);
    SceneDeckFrame result;
    result.output_ = current_->Tick(monotonic_seconds, false, extent, renderer, current_inputs,
                                    current_time);
    if (!incoming_) return result;
    if (!warmed_) incoming_origin_ = seconds;
    const double incoming_seconds = std::max(0.0, seconds - incoming_origin_);
    const runtime::PlaybackSample incoming_time{
            incoming_seconds, scene_generation_, master_.Paused(), {}};
    try {
        // Both scenes share backend admission. A rejected incoming frame never
        // becomes the public output and never replaces the active session.
        auto incoming_inputs = inputs;
        // Public macro IDs belong to the active work, unlike shared audio/input
        // snapshots. The incoming work starts from its own authored controls.
        incoming_inputs.controls_.clear();
        const auto next = incoming_->Tick(monotonic_seconds, false,
                                          PlaybackExtent(incoming_->Canvas(), quality), renderer,
                                          incoming_inputs, incoming_time);
        if (next.budget_) throw render::BudgetExceeded(*next.budget_);
        if (!renderer.IsValid(next.final_)) throw std::runtime_error("player.transition_output");
        if (transition_action_) actions_.Resolve(*transition_action_, true);
        transition_action_.reset();
        warmed_ = true;
        progress_ = duration_ > 0 ? std::clamp(incoming_seconds / duration_, 0.0, 1.0) : 1;
        if (progress_ < 1) {
            result.output_.final_ =
                    compositor_.Blend(renderer, {result.output_.final_, current_->Canvas()},
                                      {next.final_, incoming_->Canvas()}, extent, progress_);
        } else {
            retired_ = std::move(current_);
            current_ = std::move(incoming_);
            origin_ = incoming_origin_;
            result.output_ = next;
            result.switched_ = true;
            result.entry_seconds_ = incoming_seconds;
            ResetPerformance();
        }
    } catch (const render::BudgetExceeded&) {
        if (transition_action_) actions_.Resolve(*transition_action_, false);
        CancelTransition();
        error_ = SceneTransitionError::kBudget;
    } catch (const std::exception&) {
        if (transition_action_) actions_.Resolve(*transition_action_, false);
        CancelTransition();
        error_ = SceneTransitionError::kRender;
    }
    return result;
}
}  // namespace rhythm::player
