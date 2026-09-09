#include <cmath>
#include <iostream>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <string>

#include "rhythm/player/performance_actions.h"

namespace {
void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition)
        throw std::runtime_error("performance.contract:" + std::to_string(location.line()));
}
template <typename Callback>
void Reject(Callback callback) {
    bool rejected = false;
    try {
        callback();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    Check(rejected);
}
}  // namespace
int main() {
    using namespace rhythm;
    using Kind = player::PerformanceActionKind;
    using State = player::PerformanceActionState;
    using Reason = player::PerformanceActionReason;
    using Mode = parameters::Quantization;
    try {
        player::PerformanceActions actions;
        const parameters::BeatSettings grid;
        Reject([&] { actions.Request(Kind::kSnapshot, 1, Mode::kBeat); });
        actions.Observe({0.1, 1, false}, 7, grid);
        const auto first = actions.Request(Kind::kSnapshot, 11, Mode::kBeat);
        const auto scene = actions.Request(Kind::kNextScene, 91, Mode::kBar);
        Check(actions.Status(Kind::kSnapshot).due_seconds_ == 0.5);
        Check(actions.Status(Kind::kNextScene).due_seconds_ == 2);
        actions.Observe({0.3, 1, false}, 7, grid);
        Check(actions.Request(Kind::kSnapshot, 11, Mode::kBeat) == first);
        Check(!actions.TakeDue(Kind::kSnapshot));
        actions.Observe({std::nextafter(0.5, 0.0), 1, false}, 7, grid);
        Check(!actions.TakeDue(Kind::kSnapshot));
        actions.Observe({0.5, 1, false}, 7, grid);
        Check(actions.TakeDue(Kind::kSnapshot).value().id_ == first);
        Check(!actions.TakeDue(Kind::kSnapshot) && !actions.TakeDue(Kind::kNextScene));
        Check(actions.Resolve(first, true) && !actions.Resolve(first, true));
        Check(actions.Status(Kind::kSnapshot).state_ == State::kCompleted);
        const auto second = actions.Request(Kind::kSnapshot, 12, Mode::kBeat);
        Check(actions.Status(Kind::kSnapshot).due_seconds_ == 1);
        const auto replacement = actions.Request(Kind::kSnapshot, 13, Mode::kBar);
        Check(replacement > second && actions.Status(Kind::kSnapshot).replaced_id_ == second);
        Check(!actions.Cancel(second) && actions.Cancel(replacement));
        actions.Observe({2, 1, true}, 7, grid);
        Check(!actions.TakeDue(Kind::kNextScene));
        actions.Observe({2, 1, false}, 7, grid, true);
        Check(!actions.TakeDue(Kind::kNextScene));
        actions.Observe({2, 1, false}, 7, grid);
        Check(actions.TakeDue(Kind::kNextScene).value().id_ == scene);
        Check(actions.Resolve(scene, false));
        Check(actions.Status(Kind::kNextScene).reason_ == Reason::kTargetUnavailable);

        auto request = actions.Request(Kind::kSnapshot, 11, Mode::kBeat);
        actions.Observe({2.1, 2, false}, 7, grid);
        Check(actions.Status(Kind::kSnapshot).state_ == State::kCancelled);
        Check(actions.Status(Kind::kSnapshot).reason_ == Reason::kSourceChanged);
        Check(!actions.Resolve(request, true));
        actions.Request(Kind::kSnapshot, 11, Mode::kBeat);
        actions.Observe({2.1, 2, false}, 8, grid);
        Check(actions.Status(Kind::kSnapshot).reason_ == Reason::kSourceChanged);
        actions.Request(Kind::kSnapshot, 11, Mode::kBeat);
        actions.Observe({1, 2, false}, 8, grid);
        Check(actions.Status(Kind::kSnapshot).reason_ == Reason::kSourceChanged);
        actions.Request(Kind::kSnapshot, 11, Mode::kBeat);
        auto changed = grid;
        changed.bpm_ = 97;
        actions.Observe({1, 2, false}, 8, changed);
        Check(actions.Status(Kind::kSnapshot).reason_ == Reason::kGridChanged);
        actions.Request(Kind::kSnapshot, 11, Mode::kBeat);
        changed.origin_seconds_ = 0.1;
        actions.Observe({1, 2, false}, 8, changed);
        Check(actions.Status(Kind::kSnapshot).reason_ == Reason::kGridChanged);
        actions.Observe({1, 2, false}, 8, {});
        actions.Request(Kind::kSnapshot, 11, Mode::kBeat);
        Check(actions.Status(Kind::kSnapshot).reason_ == Reason::kNoGrid);
        request = actions.Request(Kind::kSnapshot, 11, Mode::kImmediate);
        Check(actions.TakeDue(Kind::kSnapshot).value().id_ == request);
        actions.CancelAll();
        Check(!actions.Resolve(request, true));
        actions.Observe({parameters::kMaximumBeatSeconds, 3, false}, 8, grid);
        actions.Request(Kind::kSnapshot, 11, Mode::kBeat);
        Check(actions.Status(Kind::kSnapshot).reason_ == Reason::kNoBoundary);
        const auto before_invalid = actions.Status(Kind::kSnapshot);
        Reject([&] { actions.Observe({-1, 4, false}, 9, grid); });
        Reject([&] {
            actions.Observe({std::numeric_limits<double>::infinity(), 4, false}, 9, grid);
        });
        Reject([&] { actions.Request(Kind::kSnapshot, 0, Mode::kImmediate); });
        Reject([&] { actions.Request(Kind::kSnapshot, 1, static_cast<Mode>(99)); });
        Check(actions.Status(Kind::kSnapshot) == before_invalid);

        // Display-frame dispatch happens once, never early, within one normal
        // frame after the target. Wall time does not advance a stalled sample.
        for (const double fps : {30.0, 60.0, 144.0}) {
            player::PerformanceActions timing;
            timing.Observe({0.31, 1, false}, 1, changed);
            const auto id = timing.Request(Kind::kSnapshot, 11, Mode::kBeat);
            const auto due = *timing.Status(Kind::kSnapshot).due_seconds_;
            int executions = 0;
            for (int frame = 0; frame < 200; ++frame) {
                const double seconds = 0.31 + frame / fps;
                timing.Observe({seconds, 1, false}, 1, changed);
                if (const auto action = timing.TakeDue(Kind::kSnapshot)) {
                    Check(action->id_ == id && seconds >= due && seconds - due <= 1 / fps);
                    Check(timing.Resolve(id, true));
                    ++executions;
                }
            }
            Check(executions == 1);
        }
        std::cout << "Performance actions: bounded replacement, cancellation, pause, generation, "
                     "failure, strict beat boundary and 30/60/144 Hz dispatch passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
