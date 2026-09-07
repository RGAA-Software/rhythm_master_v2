#pragma once

#include "rhythm/player/prepared_package.h"
#include "rhythm/project/package.h"
#include "rhythm/runtime/frame_clock.h"
#include "rhythm/runtime/runtime.h"

namespace rhythm::player {
// Host-thread confined playback. Loading validates before replacing the current
// package. A session owns simulation/GPU state, while the host owns presentation.
class Session final {
   public:
    void Load(std::string_view package_bytes);
    // Nonzero initial time requires a validated stateless package; no implicit
    // feedback/history reconstruction is performed for late scene joins.
    void LoadPrepared(PreparedPackage package, double initial_seconds = 0);
    void Open(const std::filesystem::path& path);
    void SetPaused(bool paused) { paused_ = paused; }
    [[nodiscard]] bool Paused() const { return paused_; }
    [[nodiscard]] bool Ready() const { return package_.has_value(); }
    [[nodiscard]] const std::string& Title() const;
    [[nodiscard]] render::Extent Canvas() const;
    [[nodiscard]] double Seconds() const { return seconds_; }
    void Restart();
    // Completed-frame boundary only; release before destroying a host device.
    // Playback time survives surface replacement, simulation history is reset.
    void ReleaseGraphics();
    // One immutable input snapshot per host-thread frame. Pause/suspension hold
    // the last evaluated snapshot, including during surface replacement. Missing
    // external session time selects local playback; Seconds() stays local time.
    runtime::FrameResult Tick(double monotonic_seconds, bool suspended, render::Extent extent,
                              render::Renderer& renderer,
                              const runtime::ExternalInputs& inputs = {});

   private:
    void Commit(project::RuntimePackage package, std::shared_ptr<const scene::Resources> resources);
    std::optional<project::RuntimePackage> package_{};
    std::shared_ptr<const scene::Resources> resources_{};
    runtime::Runtime runtime_{};
    runtime::FrameClock clock_{};
    runtime::FrameResult frame_{};
    runtime::ExternalInputs external_{};
    render::Extent extent_{};
    double seconds_ = 0;
    double playback_offset_seconds_ = 0;
    std::uint64_t generation_ = 0;
    bool paused_ = false;
};
}  // namespace rhythm::player
