#include <iostream>
#include <stdexcept>

#include "rhythm/cluster/scene_coordinator.h"

int main() {
    using namespace rhythm::cluster;
    try {
        const auto check = [](bool value) {
            if (!value) throw std::runtime_error("cluster.scene_contract");
        };
        SceneCoordinator room(3);
        check(room.AddPeer(1, 1) && room.AddPeer(2, 1) && room.AddPeer(3, 2));
        check(!room.AddPeer(4, 1));
        SceneIdentity scene{1};
        scene.package_hash_[0] = 1;
        check(room.Prepare(scene, 0, 100000));
        check(room.Peers()[2].state_ == ScenePeerState::kWaiting && !room.Peers()[2].compatible_);
        check(!room.Ready(3, scene, 1000));
        auto wrong = scene;
        wrong.package_hash_[0] = 2;
        check(!room.Ready(1, wrong, 1000));
        check(room.Ready(1, scene, 1000));
        check(!room.Ready(1, scene, 1000));
        check(!room.Commit(99000, 200000, false));
        check(!room.Commit(100000, 200000, true));
        check(room.Peers()[0].state_ == ScenePeerState::kReady);
        const auto commit = room.Commit(100000, 200000, false).value();
        check(commit.peers_ == std::vector<std::uint64_t>{1});
        check(commit.origin_us_ == 200000);
        check(!room.Commit(100000, 200000, false));
        check(!room.Acknowledge(1, scene, 200001));
        check(room.Acknowledge(1, scene, 200000));
        check(!room.Acknowledge(1, scene, 200000));
        check(room.Ready(2, scene, 150000));
        const auto late = room.JoinLate(2, 160000, 250000).value();
        check(late.peers_ == std::vector<std::uint64_t>{2} && late.start_at_us_ == 250000);
        check(late.origin_us_ == commit.origin_us_);
        check(!room.JoinLate(2, 160000, 250000));
        check(room.RemovePeer(1) && !room.RemovePeer(1));
        check(!room.AddPeer(1, 1) && room.AddPeer(4, 1));
        check(room.Peers().back().state_ == ScenePeerState::kPreparing);
        auto next = scene;
        next.generation_ = 2;
        check(room.Prepare(next, 200000, 300000));
        check(!room.Ready(4, scene, 210000) && !room.Acknowledge(2, scene, 250000));
        check(room.Ready(4, next, 210000));
        check(!room.Commit(300000, 310000, false));
        check(room.Commit(300000, 350000, false).has_value());
        room.Stop();
        check(!room.Acknowledge(4, next, 350000) && !room.Ready(2, next, 400000));
        check(!room.Prepare(next, 400000, 400000));
        next.generation_ = 3;
        check(!room.Prepare(next, 299999, 400000));
        check(room.Prepare(next, 400000, 400000));
        check(!room.Commit(400000, 450000, false));
        SceneCoordinator maximum(1000);
        for (std::uint64_t peer = 1; peer <= 1000; ++peer) check(maximum.AddPeer(peer, 1));
        check(!maximum.AddPeer(1001, 1));
        check(maximum.Prepare(scene, 0, 1000));
        for (std::uint64_t peer = 1; peer <= 1000; ++peer) check(maximum.Ready(peer, scene, 1000));
        check(maximum.Commit(1000, 100000, true)->peers_.size() == 1000);
        SceneCoordinator deadline(2);
        check(deadline.AddPeer(1, 1) && deadline.AddPeer(2, 1));
        check(deadline.Prepare(scene, 0, 1000));
        check(deadline.Ready(1, scene, 1000) && deadline.Ready(2, scene, 1001));
        check(!deadline.Commit(2000, 100000, true));
        check(deadline.Commit(2000, 100000, false)->peers_ == std::vector<std::uint64_t>{1});
        check(deadline.JoinLate(2, 2000, 150000)->peers_ == std::vector<std::uint64_t>{2});
        std::cout << "scene contracts passed: generation/hash/profile, readiness deadline, "
                     "partial/all commit, late join, stop, peer budget\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
