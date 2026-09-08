#pragma once

#include "rhythm/player/render_quality.h"
#include "rhythm/player/scene_compositor.h"
#include "rhythm/player/session.h"

namespace rhythm::player {
enum class SceneTransitionError { kNone, kBusy, kInvalid, kBudget, kRender, kDiscontinuity };
struct SceneDeckFrame {
    runtime::FrameResult output_{};
    bool switched_ = false;
    double entry_seconds_ = 0;
};
// Host-thread performance owner. One master clock drives both sessions using
// derived scene-local samples; it owns no decoder/device/transport. At most two
// GPU sessions coexist. Mutations precede Tick; returned handles stay alive
// through EndFrame. Retired output owners are released at the next Tick.
class SceneDeck final {
   public:
    const Session& Current() const { return *current_; }
    void Open(const std::filesystem::path& path);
    void LoadPrepared(PreparedPackage package);
    bool StartTransition(PreparedPackage package, double duration);
    void SetPaused(bool paused);
    void Seek(double seconds);
    void Restart() { Seek(0); }
    void CancelTransition();
    void ReleaseGraphics();
    // After loading a newly promoted work's soundtrack, seek that audio service
    // to entry_seconds_, then pass its immediate value snapshot here. The next
    // frame rebases the master without discarding the incoming scene's history.
    void AdoptMedia(const runtime::PlaybackSample& sample);
    SceneDeckFrame Tick(double monotonic_seconds, bool suspended, RenderQuality quality,
                        render::Renderer& renderer, const runtime::ExternalInputs& inputs = {},
                        const std::optional<runtime::PlaybackSample>& playback = {});
    bool Transitioning() const { return bool(incoming_); }
    bool CanPrepareNext() const { return !incoming_ && !retired_; }
    double Progress() const { return progress_; }
    std::string IncomingTitle() const { return incoming_ ? incoming_->Title() : std::string{}; }
    SceneTransitionError Error() const { return error_; }

   private:
    void ResetClock();
    std::unique_ptr<Session> current_ = std::make_unique<Session>();
    std::unique_ptr<Session> incoming_{};
    std::unique_ptr<Session> retired_{};
    SceneCompositor compositor_{};
    runtime::PlaybackClock master_{};
    std::uint64_t master_generation_ = 0;
    std::uint64_t scene_generation_ = 1;
    std::optional<std::uint64_t> handoff_generation_{};
    double origin_ = 0;
    double incoming_origin_ = 0;
    double duration_ = 0;
    double progress_ = 0;
    bool warmed_ = false;
    bool clock_observed_ = false;
    SceneTransitionError error_ = SceneTransitionError::kNone;
};
}  // namespace rhythm::player
