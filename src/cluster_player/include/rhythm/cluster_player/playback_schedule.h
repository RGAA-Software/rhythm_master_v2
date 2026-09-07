#pragma once

#include "rhythm/cluster/scene_coordinator.h"
#include "rhythm/player/session.h"

namespace rhythm::cluster_player {
enum class ScheduleStep { kIdle, kWaiting, kApplied, kMissed, kInvalidTime };
// One authenticated room epoch, host-thread calls only. The caller supplies a
// synchronized monotonic host time and reconstructs this object on room epoch
// change. Network, file loading and rendering ownership remain in their adapters.
class PlaybackSchedule final {
   public:
    bool Prepare(cluster::SceneIdentity scene);
    bool Install(const cluster::SceneIdentity& scene, player::PreparedPackage package);
    bool Ready(const cluster::SceneIdentity& scene) const;
    bool Commit(const cluster::SceneIdentity& scene, std::int64_t start_at_us,
                std::int64_t origin_us, std::int64_t host_now_us);
    // Apply before frame texture handles are borrowed. More than 250 ms late
    // misses this commit and retains current playback; a new commit is required.
    ScheduleStep ApplyAt(std::int64_t host_now_us, player::Session& session);
    // Cancels pending work; current Session playback is deliberately independent.
    void CancelPending();
    std::optional<std::int64_t> ActiveOriginUs() const { return active_origin_us_; }

   private:
    struct Timing {
        std::int64_t start_at_us_ = 0;
        std::int64_t origin_us_ = 0;
    };
    bool Advance(std::int64_t host_now_us);
    std::optional<cluster::SceneIdentity> pending_scene_{};
    std::optional<player::PreparedPackage> prepared_{};
    std::optional<Timing> timing_{};
    std::optional<std::int64_t> last_host_us_{};
    std::optional<std::int64_t> active_origin_us_{};
    std::uint64_t generation_ = 0;
    std::int64_t last_start_us_ = 0;
};
}  // namespace rhythm::cluster_player
