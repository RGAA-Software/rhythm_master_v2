#include "rhythm/player/scene_deck.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "scene_audio_clock.h"

namespace rhythm::player {
SceneDeck::SceneDeck() : audio_clock_(std::make_unique<SceneAudioClock>()) {}
SceneDeck::~SceneDeck() = default;
void SceneDeck::EnableAudioTransitions(bool enabled) {
    if (enabled != audio_enabled_ && !CanPrepareNext())
        throw std::logic_error("player.transition_busy");
    audio_enabled_ = enabled;
}
std::uint64_t SceneDeck::AudioPendingId() const { return audio_clock_->Id(); }
bool SceneDeck::AudioReady() const { return Transitioning() && warmed_ && audio_clock_->Active(); }
bool SceneDeck::CanPrepareNext() const {
    return !transition_started_ && !retired_ && !audio_clock_->Active();
}
std::optional<media::SoundtrackSource> SceneDeck::IncomingSoundtrack() const {
    return incoming_ ? incoming_->Soundtrack() : std::nullopt;
}
void SceneDeck::ResetClock() {
    current_passes_ = 0;
    master_ = {};
    master_generation_ = master_.Generation();
    ++scene_generation_;
    origin_ = 0;
    clock_observed_ = false;
    handoff_generation_.reset();
    DiscardTransition();
    audio_clock_->Reset();
    media_observed_ = false;
    preserve_audio_origin_ = false;
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
bool SceneDeck::StartTransition(PreparedPackage package, double duration, bool synchronize_audio) {
    if (!CanPrepareNext() || incoming_) {
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
    warmed_ = false;
    preparation_ = {};
    ActivateTransition(duration, synchronize_audio);
    return true;
}
void SceneDeck::ActivateTransition(double duration, bool synchronize_audio) {
    ++transition_id_;
    if (synchronize_audio || (audio_enabled_ && incoming_->Soundtrack()))
        audio_clock_->Begin(transition_id_, duration, scene_generation_);
    duration_ = duration;
    progress_ = 0;
    transition_started_ = true;
    origin_set_ = false;
    error_ = SceneTransitionError::kNone;
    error_detail_.clear();
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
    DiscardTransition();
    audio_clock_->Reset();
    preserve_audio_origin_ = false;
    actions_.CancelAll(PerformanceActionReason::kSourceChanged);
}
void SceneDeck::CancelTransition() {
    if (incoming_ && audio_clock_->Active()) {
        if (transition_action_) actions_.Cancel(*transition_action_);
        transition_action_.reset();
        cancel_requested_ = true;
        progress_ = 0;
        return;
    }
    DiscardTransition();
}
void SceneDeck::DiscardTransition() {
    if (queue_id_) discarded_queue_id_ = queue_id_;
    queue_id_ = 0;
    transition_started_ = false;
    origin_set_ = false;
    if (transition_action_) actions_.Cancel(*transition_action_);
    transition_action_.reset();
    incoming_.reset();
    cancel_requested_ = false;
    compositor_.ReleaseGraphics();
    progress_ = 0;
    warmed_ = false;
    preparation_ = {};
    error_ = SceneTransitionError::kNone;
    error_detail_.clear();
}
void SceneDeck::ReleaseGraphics() {
    current_->ReleaseGraphics();
    if (incoming_) {
        incoming_->ReleaseGraphics();
        warmed_ = false;
        preparation_ = {};
    }
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
void SceneDeck::AnchorMedia(const runtime::PlaybackSample& sample) {
    if (clock_observed_ || !CanPrepareNext() || !std::isfinite(sample.seconds_) ||
        sample.seconds_ < 0 ||
        (sample.duration_ && (!std::isfinite(*sample.duration_) || *sample.duration_ <= 0)))
        throw std::invalid_argument("player.media_anchor_position");
    origin_ = sample.seconds_ - current_->Seconds();
    handoff_generation_ = sample.generation_;
}
SceneDeckFrame SceneDeck::Tick(double monotonic_seconds, bool suspended, RenderQuality quality,
                               render::Renderer& renderer, const runtime::ExternalInputs& inputs,
                               const std::optional<runtime::PlaybackSample>& playback,
                               const std::optional<std::reference_wrapper<SceneQueue>>& queue,
                               const std::optional<SceneAudioSample>& audio) {
    if (!runtime::ValidExternalInputs(inputs))
        throw std::invalid_argument("runtime.external_inputs");
    if (retired_) {
        retired_.reset();
        compositor_.ReleaseGraphics();
    }
    const auto seconds = master_.Advance(monotonic_seconds, suspended, playback);
    if (suspended) return {};
    bool synchronized = audio_clock_->Active();
    if (synchronized && !media_observed_ && playback) preserve_audio_origin_ = true;
    media_observed_ = playback.has_value();
    if (master_generation_ != master_.Generation()) {
        master_generation_ = master_.Generation();
        if (!synchronized &&
            !(handoff_generation_ && playback && *handoff_generation_ == playback->generation_))
            actions_.CancelAll(PerformanceActionReason::kSourceChanged);
        if (clock_observed_ && !synchronized &&
            !(handoff_generation_ && playback && *handoff_generation_ == playback->generation_)) {
            const bool interrupted = Transitioning();
            DiscardTransition();
            origin_ = 0;
            ++scene_generation_;
            if (interrupted) error_ = SceneTransitionError::kDiscontinuity;
        }
    }
    clock_observed_ = true;
    handoff_generation_.reset();
    if (!current_->Ready()) return {};
    runtime::PlaybackSample current_time{
            std::max(0.0, seconds - origin_), scene_generation_, master_.Paused(), {}};
    if (synchronized && preserve_audio_origin_ && !audio_clock_->PreviousOffset())
        current_time.seconds_ = current_->Seconds();
    auto audio_frame = audio_clock_->Step(current_time, audio);
    if (synchronized) {
        current_time = audio_frame.previous_;
        scene_generation_ = current_time.generation_;
    }
    // A quality change invalidates queue readiness before a due Go can consume it.
    if (queue_id_ && incoming_ && warmed_ &&
        PlaybackExtent(incoming_->Canvas(), quality) != preparation_extent_) {
        incoming_->ReleaseGraphics();
        warmed_ = false;
    }
    ApplyPerformance(current_time, queue);
    if (!synchronized && audio_clock_->Active()) {
        synchronized = true;
        audio_frame = audio_clock_->Step(current_time, audio);
    }
    if (synchronized && audio_frame.abort_ && incoming_) {
        if (transition_action_) actions_.Resolve(*transition_action_, false);
        DiscardTransition();
        if (audio && (audio->phase_ == SceneAudioPhase::kFailed || !audio->error_.empty())) {
            error_ = SceneTransitionError::kAudio;
            error_detail_ = audio->error_;
        }
    }
    auto current_inputs = inputs;
    for (const auto& [id, value] : live_controls_) current_inputs.controls_[id] = value;
    const auto extent = PlaybackExtent(current_->Canvas(), quality);
    SceneDeckFrame result;
    const auto previous_passes = renderer.Stats().passes_;
    result.output_ = current_->Tick(monotonic_seconds, false, extent, renderer, current_inputs,
                                    current_time);
    current_passes_ = std::max(current_passes_, renderer.Stats().passes_ - previous_passes);
    if (synchronized && !incoming_ && audio_frame.terminal_) {
        if (const auto offset = audio_clock_->PreviousOffset()) origin_ = -*offset;
        if (playback) handoff_generation_ = playback->generation_;
        audio_clock_->Reset();
        preserve_audio_origin_ = false;
    }
    PrepareQueue(monotonic_seconds, quality, renderer, result.output_.final_, inputs, queue);
    if (!incoming_ || !transition_started_ || (cancel_requested_ && !audio_frame.committed_))
        return result;
    if (!warmed_ || !origin_set_) {
        incoming_origin_ = seconds;
        origin_set_ = true;
    }
    const double incoming_seconds = std::max(0.0, seconds - incoming_origin_);
    runtime::PlaybackSample incoming_time{
            incoming_seconds, scene_generation_, master_.Paused(), {}};
    if (synchronized) incoming_time = audio_frame.incoming_;
    try {
        // Both scenes share backend admission. A rejected incoming frame never
        // becomes the public output and never replaces the active session.
        auto incoming_inputs = inputs;
        // Public macro IDs belong to the active work, unlike shared audio/input
        // snapshots. The incoming work starts from its own authored controls.
        incoming_inputs.controls_.clear();
        runtime::FrameResult next;
        if (!warmed_) {
            auto fixed_time = incoming_time;
            fixed_time.paused_ = true;
            preparation_ = incoming_->PrepareGraphics(monotonic_seconds,
                                                      PlaybackExtent(incoming_->Canvas(), quality),
                                                      renderer, incoming_inputs, fixed_time);
            if (preparation_.state_ == runtime::PreparationState::kPending) return result;
            if (preparation_.budget_) throw render::BudgetExceeded(*preparation_.budget_);
            if (preparation_.state_ == runtime::PreparationState::kFailed)
                throw std::runtime_error(preparation_.error_);
            if (preparation_.required_passes_ + current_passes_ + 1 >
                render::kMaximumOffscreenPasses)
                throw render::BudgetExceeded(render::Budget::kPasses);
            next = *preparation_.output_;
        } else {
            next = incoming_->Tick(monotonic_seconds, false,
                                   PlaybackExtent(incoming_->Canvas(), quality), renderer,
                                   incoming_inputs, incoming_time);
        }
        if (next.budget_) throw render::BudgetExceeded(*next.budget_);
        if (!renderer.IsValid(next.final_)) throw std::runtime_error("player.transition_output");
        if (!synchronized || audio_frame.running_ || audio_frame.committed_) {
            if (transition_action_) actions_.Resolve(*transition_action_, true);
            transition_action_.reset();
        }
        warmed_ = true;
        progress_ =
                synchronized
                        ? audio_frame.progress_
                        : (duration_ > 0 ? std::clamp(incoming_seconds / duration_, 0.0, 1.0) : 1);
        if (synchronized && !audio_frame.running_ && !audio_frame.committed_) return result;
        if (synchronized ? !audio_frame.committed_ : progress_ < 1) {
            result.output_.final_ =
                    compositor_.Blend(renderer, {result.output_.final_, current_->Canvas()},
                                      {next.final_, incoming_->Canvas()}, extent, progress_);
        } else {
            retired_ = std::move(current_);
            current_ = std::move(incoming_);
            current_passes_ = preparation_.required_passes_;
            transition_started_ = false;
            cancel_requested_ = false;
            origin_ = incoming_origin_;
            result.output_ = next;
            result.switched_ = true;
            result.entry_seconds_ = incoming_time.seconds_;
            result.transition_id_ = transition_id_;
            result.audio_synchronized_ = synchronized;
            if (synchronized) {
                origin_ = 0;
                scene_generation_ = incoming_time.generation_;
                if (playback) handoff_generation_ = playback->generation_;
                audio_clock_->Reset();
                preserve_audio_origin_ = false;
            }
            ResetPerformance();
        }
    } catch (const render::BudgetExceeded&) {
        if (transition_action_) actions_.Resolve(*transition_action_, false);
        DiscardTransition();
        error_ = SceneTransitionError::kBudget;
    } catch (const std::exception& error) {
        if (transition_action_) actions_.Resolve(*transition_action_, false);
        DiscardTransition();
        error_ = SceneTransitionError::kRender;
        error_detail_ = error.what();
    }
    return result;
}
}  // namespace rhythm::player
