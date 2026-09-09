#include <iostream>
#include <stdexcept>

#include "rhythm/parameters/event_track.h"

namespace {
void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
template <typename Callback>
void Reject(Callback callback) {
    bool rejected = false;
    try {
        callback();
    } catch (const std::exception&) {
        rejected = true;
    }
    Check(rejected, "invalid recorded action accepted");
}
}  // namespace
int main() {
    using namespace rhythm::parameters;
    try {
        EventTrack track(
                {{3, 1}, {1, 0}, {2, 0.5, EventKind::kGate, 0}, {4, 1, EventKind::kReset}});
        Check(track.Events()[0].id_ == 1 && track.Events()[2].id_ == 3,
              "track order is not stable by time and identity");
        Check(track.Between(0, 0.5).size() == 1 && track.Between(0.5, 1).size() == 2 &&
                      track.Between(1, 1).empty(),
              "track interval repeated or lost boundary actions");
        for (const int rate : {30, 60, 144}) {
            std::vector<std::uint64_t> ids;
            for (int frame = 1; frame <= rate * 2; ++frame)
                for (const auto& event : track.Between((frame - 1.0) / rate, double(frame) / rate))
                    ids.push_back(event.id_);
            Check(ids == std::vector<std::uint64_t>{2, 3, 4}, "replay depends on frame rate");
        }
        Reject([] { EventTrack({{1, 0}, {1, 1}}); });
        Reject([] { EventTrack({{1, -1}}); });
        Reject([] { EventTrack({{1, 0, EventKind::kGate, 0.5}}); });
        Reject([] { EventTrack({{1, 0, EventKind::kReset, 0}}); });
        Reject([&] { track.Between(1, 0); });
        EventRecorder recorder;
        const EventSource source{0, 42, EventOrigin::kManual};
        Event event{2, source, 8, 5, EventKind::kPulse, 0.7};
        Check(recorder.Capture(event) == RecordingAdmission::kNotRecording, "inactive capture");
        recorder.Begin(track, source, 5, 2);
        Reject([&] { recorder.Begin({}, source, 5, 2); });
        Check(recorder.Capture(event) == RecordingAdmission::kRecorded, "dispatched action lost");
        Check(recorder.Capture(event) == RecordingAdmission::kDuplicateOrOld, "duplicate capture");
        ++event.sequence_;
        event.source_.node_ = 43;
        Check(recorder.Capture(event) == RecordingAdmission::kWrongSource, "wrong target captured");
        event.source_ = source;
        event.generation_ = 6;
        Check(recorder.Capture(event) == RecordingAdmission::kWrongGeneration, "seek crossed take");
        event.generation_ = 5;
        event.seconds_ = 1.5;
        Check(recorder.Capture(event) == RecordingAdmission::kTimeMovedBack,
              "backward time accepted");
        event.seconds_ = 2.5;
        event.kind_ = EventKind::kGate;
        event.value_ = 0;
        Check(recorder.Capture(event) == RecordingAdmission::kRecorded,
              "rejection consumed identity");
        const auto recorded = recorder.Finish();
        Check(!recorder.Active() && recorded.Events().size() == 6 && track.Events().size() == 4 &&
                      recorded.Events()[4] == RecordedEvent{5, 2, EventKind::kPulse, 0.7} &&
                      recorded.Events()[5] == RecordedEvent{6, 2.5, EventKind::kGate, 0},
              "recording mutated base or lost media time, type, value and stable record ID");
        recorder.Begin(recorded, source, 5, 3);
        recorder.Cancel();
        Check(!recorder.Active() && recorder.Captured() == 0, "cancel retained take");
        std::vector<RecordedEvent> full;
        for (std::uint64_t id = 1; id <= EventTrack::kMaximumEvents; ++id) full.push_back({id, 0});
        recorder.Begin(EventTrack(full), source, 5, 0);
        Check(recorder.Capture(event) == RecordingAdmission::kFull && recorder.Captured() == 0,
              "recording silently exceeded bounded storage");
        full.push_back({EventTrack::kMaximumEvents + 1, 0});
        Reject([&] { EventTrack over_limit(full); });
        std::cout << "Recorded actions: typed values, timestamp replay, stable IDs, isolated "
                     "takes, rejection and bounded storage passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
