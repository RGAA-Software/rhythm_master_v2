#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace rhythm::cluster {
struct SceneIdentity {
    std::uint64_t generation_ = 0;
    std::array<std::uint8_t, 32> package_hash_{};
    std::uint32_t profile_ = 0;
    bool operator==(const SceneIdentity&) const = default;
};
struct SceneCommit {
    SceneIdentity scene_{};
    std::int64_t start_at_us_ = 0;
    std::vector<std::uint64_t> peers_{};
    // Original scene timeline origin, retained for late join/recovery. A late
    // member must seek/warm up/restore as its validated package policy permits.
    std::int64_t origin_us_ = 0;
};
enum class ScenePeerState { kWaiting, kPreparing, kReady, kCommitted, kAcknowledged };
struct ScenePeerStatus {
    std::uint64_t id_ = 0;
    ScenePeerState state_ = ScenePeerState::kWaiting;
    bool compatible_ = false;
};
// Owner-thread orchestration after authenticated admission. Peer IDs are unique
// within this coordinator/room epoch, strictly increasing and never reused. Profiles use negotiated
// bit positions 0..31; profile 0 is currently texture-signal-v2. This layer owns
// readiness and commit membership, not transport, files or rendering.
class SceneCoordinator final {
   public:
    explicit SceneCoordinator(std::size_t maximum_peers = 100);
    bool AddPeer(std::uint64_t peer, std::uint32_t supported_profiles);
    bool RemovePeer(std::uint64_t peer);
    bool Prepare(SceneIdentity scene, std::int64_t now_us, std::int64_t deadline_us);
    bool Ready(std::uint64_t peer, const SceneIdentity& scene, std::int64_t now_us);
    // At/after the preparation deadline, select ready members. require_all means
    // every current member must be compatible and ready; otherwise commit fails.
    std::optional<SceneCommit> Commit(std::int64_t now_us, std::int64_t start_at_us,
                                      bool require_all);
    // Late-ready members get an explicit future commit without moving the start
    // of already committed members. They never join by guessing a missed commit.
    std::optional<SceneCommit> JoinLate(std::uint64_t peer, std::int64_t now_us,
                                        std::int64_t start_at_us);
    bool Acknowledge(std::uint64_t peer, const SceneIdentity& scene, std::int64_t start_at_us);
    std::vector<ScenePeerStatus> Peers() const;
    void Stop();

   private:
    struct Peer {
        std::uint32_t profiles_ = 0;
        ScenePeerState state_ = ScenePeerState::kWaiting;
        std::int64_t start_at_us_ = 0;
        std::int64_t ready_at_us_ = 0;
    };
    bool Compatible(const Peer& peer) const;
    bool Advance(std::int64_t now_us);
    std::map<std::uint64_t, Peer> peers_{};
    std::optional<SceneIdentity> scene_{};
    std::optional<std::int64_t> last_now_us_{};
    std::size_t maximum_peers_ = 100;
    std::uint64_t last_peer_ = 0;
    std::uint64_t last_generation_ = 0;
    std::int64_t deadline_us_ = 0;
    std::int64_t origin_us_ = 0;
    bool committed_ = false;
};
}  // namespace rhythm::cluster
