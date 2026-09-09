#pragma once

#include <functional>

#include "rhythm/player/performance_actions.h"
#include "rhythm/player/render_quality.h"
#include "rhythm/player/scene_audio.h"
#include "rhythm/player/scene_compositor.h"
#include "rhythm/player/scene_queue.h"
#include "rhythm/player/session.h"

namespace rhythm::player {
class SceneAudioClock;
enum class SceneTransitionError {
    kNone,
    kBusy,
    kInvalid,
    kBudget,
    kRender,
    kDiscontinuity,
    kAudio
};
struct SceneDeckFrame {
    runtime::FrameResult output_{};
    bool switched_ = false;
    double entry_seconds_ = 0;
    std::uint64_t transition_id_ = 0;
    bool audio_synchronized_ = false;
};
// Host-thread performance owner. One master clock drives both sessions using
// derived scene-local samples; it owns no decoder/device/transport. At most two
// GPU sessions coexist. Mutations precede Tick; returned handles stay alive
// through EndFrame. Retired output owners are released at the next Tick.
class SceneDeck final {
   public:
    SceneDeck();
    ~SceneDeck();
    SceneDeck(const SceneDeck&) = delete;
    SceneDeck& operator=(const SceneDeck&) = delete;
    const Session& Current() const { return *current_; }
    void Open(const std::filesystem::path& path);
    void LoadPrepared(PreparedPackage package);
    bool StartTransition(PreparedPackage package, double duration, bool synchronize_audio = false);
    // Host opts in after wiring an audio service. Queue transitions with an
    // authored soundtrack then wait for consumed-audio snapshots.
    void EnableAudioTransitions(bool enabled);
    std::uint64_t TransitionId() const { return transition_id_; }
    std::uint64_t AudioPendingId() const;
    bool AudioReady() const;
    double TransitionDuration() const { return duration_; }
    std::optional<media::SoundtrackSource> IncomingSoundtrack() const;
    void SetPaused(bool paused);
    void Seek(double seconds);
    void Restart() { Seek(0); }
    // Audio cancellation is acknowledged asynchronously. Retain the candidate
    // until recovery starts; a completed audible handoff wins a late cancel.
    void CancelTransition();
    void ReleaseGraphics();
    const std::optional<parameters::BeatSettings>& BeatGrid() const { return beat_grid_; }
    void SetBeatGrid(std::optional<parameters::BeatSettings> grid);
    void EditControls(const parameters::ControlValues& changes);
    void FollowCues();
    bool HasControlOverrides() const { return !live_controls_.empty(); }
    std::uint64_t RequestSnapshot(std::uint64_t snapshot, parameters::Quantization mode);
    std::uint64_t RequestNextScene(std::uint64_t queue_item, double duration,
                                   parameters::Quantization mode);
    bool CancelAction(std::uint64_t id) { return actions_.Cancel(id); }
    const PerformanceAction& ActionStatus(PerformanceActionKind kind) const {
        return actions_.Status(kind);
    }
    // After loading a newly promoted work's soundtrack, seek that audio service
    // to entry_seconds_, then pass its immediate value snapshot here. The next
    // frame rebases the master without discarding the incoming scene's history.
    void AdoptMedia(const runtime::PlaybackSample& sample);
    // Keep a newly selected visual-only work at its own time while retaining
    // already-playing music. Call before its first Tick after LoadPrepared.
    void AnchorMedia(const runtime::PlaybackSample& sample);
    SceneDeckFrame Tick(double monotonic_seconds, bool suspended, RenderQuality quality,
                        render::Renderer& renderer, const runtime::ExternalInputs& inputs = {},
                        const std::optional<runtime::PlaybackSample>& playback = {},
                        const std::optional<std::reference_wrapper<SceneQueue>>& queue = {},
                        const std::optional<SceneAudioSample>& audio = {});
    bool Transitioning() const {
        return bool(incoming_) && transition_started_ && !cancel_requested_;
    }
    bool CanPrepareNext() const;
    bool QueueReady(std::uint64_t id) const {
        return incoming_ && queue_id_ == id && warmed_ && !transition_started_;
    }
    bool PreparingGraphics() const { return incoming_ && !warmed_ && !cancel_requested_; }
    const runtime::PreparationProgress& GraphicsPreparation() const { return preparation_; }
    double Progress() const { return progress_; }
    std::string IncomingTitle() const { return incoming_ ? incoming_->Title() : std::string{}; }
    SceneTransitionError Error() const { return error_; }
    const std::string& ErrorDetail() const { return error_detail_; }

   private:
    void ResetClock();
    void DiscardTransition(std::string reason = "player.transition_cancelled");
    void ReportQueueOutcome(const std::optional<std::reference_wrapper<SceneQueue>>& queue);
    void ResetPerformance();
    void ActivateTransition(double duration, bool synchronize_audio = false);
    void PrepareQueue(double monotonic_seconds, RenderQuality quality, render::Renderer& renderer,
                      render::TextureHandle current_output, const runtime::ExternalInputs& inputs,
                      const std::optional<std::reference_wrapper<SceneQueue>>& queue);
    void ApplyPerformance(const runtime::PlaybackSample& sample,
                          const std::optional<std::reference_wrapper<SceneQueue>>& queue);
    std::unique_ptr<Session> current_ = std::make_unique<Session>();
    std::unique_ptr<Session> incoming_{};
    std::unique_ptr<Session> retired_{};
    SceneCompositor compositor_{};
    std::unique_ptr<SceneAudioClock> audio_clock_{};
    runtime::PlaybackClock master_{};
    std::uint64_t master_generation_ = 0;
    std::uint64_t scene_generation_ = 1;
    std::optional<std::uint64_t> handoff_generation_{};
    double origin_ = 0;
    double incoming_origin_ = 0;
    double duration_ = 0;
    double progress_ = 0;
    bool warmed_ = false;
    bool cancel_requested_ = false;
    bool clock_observed_ = false;
    SceneTransitionError error_ = SceneTransitionError::kNone;
    PerformanceActions actions_{};
    std::optional<parameters::BeatSettings> beat_grid_{};
    parameters::ControlValues live_controls_{};
    std::uint64_t performance_generation_ = 0;
    std::optional<std::uint64_t> transition_action_{};
    double requested_duration_ = 0;
    std::uint64_t transition_id_ = 0;
    bool audio_enabled_ = false;
    bool media_observed_ = false;
    bool preserve_audio_origin_ = false;
    std::string error_detail_{};
    runtime::PreparationProgress preparation_{};
    std::uint32_t current_passes_ = 0;
    bool transition_started_ = false;
    bool origin_set_ = false;
    std::uint64_t queue_id_ = 0;
    std::uint64_t active_queue_id_ = 0;
    struct QueueOutcome {
        std::uint64_t id_ = 0;
        bool accepted_ = false;
        std::string error_{};
    };
    std::optional<QueueOutcome> queue_outcome_{};
    std::uint64_t discarded_queue_id_ = 0;
    render::Extent preparation_extent_{};
};
}  // namespace rhythm::player
