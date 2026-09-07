#include "rhythm/cluster/scene_coordinator.h"

#include <algorithm>
#include <stdexcept>

namespace rhythm::cluster {
namespace {
constexpr std::int64_t kMaximumTime = std::int64_t{1} << 52;
bool ValidTime(std::int64_t time) { return time >= 0 && time <= kMaximumTime; }
bool ValidStart(std::int64_t now, std::int64_t start) {
    return ValidTime(now) && ValidTime(start) && start - now >= 50000 && start - now <= 30000000;
}
}  // namespace
SceneCoordinator::SceneCoordinator(std::size_t maximum_peers) : maximum_peers_(maximum_peers) {
    if (!maximum_peers || maximum_peers > 1000) throw std::invalid_argument("cluster.peer_budget");
}
bool SceneCoordinator::Compatible(const Peer& peer) const {
    return scene_ && (peer.profiles_ & (std::uint32_t{1} << scene_->profile_));
}
bool SceneCoordinator::AddPeer(std::uint64_t peer, std::uint32_t supported_profiles) {
    if (!peer || peer <= last_peer_ || !supported_profiles || peers_.size() >= maximum_peers_)
        return false;
    Peer member;
    member.profiles_ = supported_profiles;
    if (Compatible(member)) member.state_ = ScenePeerState::kPreparing;
    peers_.emplace(peer, member);
    last_peer_ = peer;
    return true;
}
bool SceneCoordinator::RemovePeer(std::uint64_t peer) { return peers_.erase(peer) != 0; }
bool SceneCoordinator::Advance(std::int64_t now_us) {
    if (!ValidTime(now_us) || (last_now_us_ && now_us < *last_now_us_)) return false;
    last_now_us_ = now_us;
    return true;
}
bool SceneCoordinator::Prepare(SceneIdentity scene, std::int64_t now_us, std::int64_t deadline_us) {
    if (!scene.generation_ || scene.generation_ <= last_generation_ || scene.profile_ > 31 ||
        std::all_of(scene.package_hash_.begin(), scene.package_hash_.end(),
                    [](auto byte) { return byte == 0; }) ||
        !ValidTime(now_us) || !ValidTime(deadline_us) || deadline_us < now_us ||
        deadline_us - now_us > 30000000 || !Advance(now_us))
        return false;
    scene_ = scene;
    last_generation_ = scene.generation_;
    deadline_us_ = deadline_us;
    origin_us_ = 0;
    committed_ = false;
    for (auto& [id, peer] : peers_) {
        (void)id;
        peer.state_ = Compatible(peer) ? ScenePeerState::kPreparing : ScenePeerState::kWaiting;
        peer.start_at_us_ = 0;
        peer.ready_at_us_ = 0;
    }
    return true;
}
bool SceneCoordinator::Ready(std::uint64_t peer, const SceneIdentity& scene, std::int64_t now_us) {
    const auto found = peers_.find(peer);
    if (!scene_ || scene != *scene_ || found == peers_.end() || !Compatible(found->second) ||
        found->second.state_ != ScenePeerState::kPreparing || !Advance(now_us))
        return false;
    found->second.state_ = ScenePeerState::kReady;
    found->second.ready_at_us_ = now_us;
    return true;
}
std::optional<SceneCommit> SceneCoordinator::Commit(std::int64_t now_us, std::int64_t start_at_us,
                                                    bool require_all) {
    if (!scene_ || committed_ || now_us < deadline_us_ || !ValidStart(now_us, start_at_us) ||
        !Advance(now_us))
        return std::nullopt;
    SceneCommit result{*scene_, start_at_us};
    for (const auto& [id, peer] : peers_) {
        if (peer.state_ == ScenePeerState::kReady && peer.ready_at_us_ <= deadline_us_)
            result.peers_.push_back(id);
        else if (require_all)
            return std::nullopt;
    }
    if (result.peers_.empty()) return std::nullopt;
    for (const auto id : result.peers_) {
        peers_.at(id).state_ = ScenePeerState::kCommitted;
        peers_.at(id).start_at_us_ = start_at_us;
    }
    committed_ = true;
    origin_us_ = start_at_us;
    result.origin_us_ = origin_us_;
    return result;
}
std::optional<SceneCommit> SceneCoordinator::JoinLate(std::uint64_t peer, std::int64_t now_us,
                                                      std::int64_t start_at_us) {
    const auto found = peers_.find(peer);
    if (!committed_ || !scene_ || found == peers_.end() ||
        found->second.state_ != ScenePeerState::kReady || !ValidStart(now_us, start_at_us) ||
        !Advance(now_us))
        return std::nullopt;
    found->second.state_ = ScenePeerState::kCommitted;
    found->second.start_at_us_ = start_at_us;
    return SceneCommit{*scene_, start_at_us, {peer}, origin_us_};
}
bool SceneCoordinator::Acknowledge(std::uint64_t peer, const SceneIdentity& scene,
                                   std::int64_t start_at_us) {
    const auto found = peers_.find(peer);
    if (!scene_ || scene != *scene_ || found == peers_.end() ||
        found->second.state_ != ScenePeerState::kCommitted ||
        found->second.start_at_us_ != start_at_us)
        return false;
    found->second.state_ = ScenePeerState::kAcknowledged;
    return true;
}
std::vector<ScenePeerStatus> SceneCoordinator::Peers() const {
    std::vector<ScenePeerStatus> result;
    result.reserve(peers_.size());
    for (const auto& [id, peer] : peers_) result.push_back({id, peer.state_, Compatible(peer)});
    return result;
}
void SceneCoordinator::Stop() {
    scene_.reset();
    committed_ = false;
    for (auto& [id, peer] : peers_) {
        (void)id;
        peer.state_ = ScenePeerState::kWaiting;
        peer.start_at_us_ = 0;
    }
}
}  // namespace rhythm::cluster
