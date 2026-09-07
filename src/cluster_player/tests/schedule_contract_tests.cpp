#include <iostream>
#include <stdexcept>

#include "rhythm/cluster_player/playback_schedule.h"

int main() {
    using namespace rhythm;
    using cluster_player::ScheduleStep;
    try {
        const auto check = [](bool value) {
            if (!value) throw std::runtime_error("cluster.schedule_contract");
        };
        graph::Registry registry;
        graph::Document graph;
        graph.id_ = "schedule.test";
        graph.output_ = 3;
        graph.nodes_ = {registry.MakeNode(1, "core.time"), registry.MakeNode(2, "texture.gradient"),
                        registry.MakeNode(3, "output.texture")};
        graph.edges_ = {{1, 1, 2, "amount"}, {2, 2, 3, "source"}};
        const auto bytes = project::EncodePackage(graph, "scheduled");
        player::PreparedPackage prepared(bytes);
        cluster::SceneIdentity scene{1, prepared.Digest(), 0};
        player::Session session;
        session.Load(project::EncodePackage(graph, "old"));
        cluster_player::PlaybackSchedule schedule;
        check(schedule.Prepare(scene));
        check(!schedule.Commit(scene, 1000000, 1000000, 0));
        auto wrong = scene;
        wrong.package_hash_[0] ^= 1;
        check(!schedule.Install(wrong, player::PreparedPackage(bytes)));
        check(schedule.Install(scene, std::move(prepared)) && schedule.Ready(scene));
        check(schedule.Prepare(scene) && schedule.Ready(scene));
        check(schedule.Commit(scene, 1000000, 1000000, 0));
        check(schedule.Commit(scene, 1000000, 1000000, 0));
        check(!schedule.Commit(scene, 2000000, 1000000, 0));
        check(schedule.ApplyAt(999999, session) == ScheduleStep::kWaiting &&
              session.Title() == "old");
        check(schedule.ApplyAt(1010000, session) == ScheduleStep::kApplied &&
              session.Title() == "scheduled");
        check(session.Seconds() == 0.01 && schedule.ActiveOriginUs() == 1000000);
        check(schedule.ApplyAt(1010000, session) == ScheduleStep::kIdle &&
              !schedule.Prepare(scene));
        scene.generation_ = 2;
        check(schedule.Prepare(scene) && schedule.Install(scene, player::PreparedPackage(bytes)));
        check(schedule.Commit(scene, 2000000, 1000000, 1100000));
        check(schedule.ApplyAt(2300001, session) == ScheduleStep::kMissed &&
              session.Seconds() == 0.01);
        check(schedule.Ready(scene) && !schedule.Commit(scene, 2000000, 1000000, 2300001));
        check(schedule.Commit(scene, 2500000, 1000000, 2300001));
        check(schedule.ApplyAt(2500000, session) == ScheduleStep::kApplied &&
              session.Seconds() == 1.5);
        check(schedule.ApplyAt(2499999, session) == ScheduleStep::kInvalidTime);
        scene.generation_ = 3;
        check(schedule.Prepare(scene));
        auto newer = scene;
        newer.generation_ = 4;
        check(schedule.Prepare(newer));
        check(!schedule.Install(scene, player::PreparedPackage(bytes)));
        check(schedule.Install(newer, player::PreparedPackage(bytes)));
        schedule.CancelPending();
        check(!schedule.Commit(newer, 3000000, 1000000, 2500000) && !schedule.Prepare(newer));
        graph.nodes_[0] = registry.MakeNode(1, "texture.feedback");
        graph.edges_ = {{1, 2, 1, "source"}, {2, 1, 3, "source"}};
        const auto feedback_bytes = project::EncodePackage(graph, "feedback");
        player::PreparedPackage feedback(feedback_bytes);
        cluster::SceneIdentity historical{5, feedback.Digest(), 0};
        check(schedule.Prepare(historical) && schedule.Install(historical, std::move(feedback)));
        check(!schedule.Commit(historical, 4000000, 1000000, 3000000));
        check(schedule.Commit(historical, 4000000, 4000000, 3000000));
        check(schedule.ApplyAt(4000000, session) == ScheduleStep::kApplied &&
              session.Seconds() == 0);
        auto profile_mismatch = scene;
        profile_mismatch.generation_ = 6;
        profile_mismatch.profile_ = 1;
        check(schedule.Prepare(profile_mismatch) &&
              !schedule.Install(profile_mismatch, player::PreparedPackage(bytes)));
        std::cout << "schedule contracts passed: prepare validation, deadlines, late offset, stale "
                     "results, feedback recovery gate\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
