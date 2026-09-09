#include "rhythm/parameters/event_track.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>
#include <tuple>

namespace rhythm::parameters {
namespace {
bool ValidTime(double seconds) { return std::isfinite(seconds) && seconds >= 0 && seconds <= 1e9; }
}  // namespace
EventTrack::EventTrack(std::vector<RecordedEvent> events, std::uint64_t last_id)
    : last_id_(last_id) {
    if (events.size() > kMaximumEvents) throw std::length_error("event.track_limit");
    std::set<std::uint64_t> ids;
    for (const auto& event : events) {
        last_id_ = std::max(last_id_, event.id_);
        if (!ids.insert(event.id_).second || !ValidEvent({event.seconds_,
                                                          {0, 1, EventOrigin::kManual},
                                                          event.id_,
                                                          1,
                                                          event.kind_,
                                                          event.value_}))
            throw std::invalid_argument("event.track_record");
    }
    std::sort(events.begin(), events.end(), [](const auto& first, const auto& second) {
        return std::tie(first.seconds_, first.id_) < std::tie(second.seconds_, second.id_);
    });
    events_ = std::move(events);
}
std::span<const RecordedEvent> EventTrack::Between(double after, double through) const {
    if (!ValidTime(after) || !ValidTime(through) || through < after)
        throw std::invalid_argument("event.track_interval");
    const auto upper = [&](double seconds) {
        return std::upper_bound(
                events_.begin(), events_.end(), seconds,
                [](double time, const auto& event) { return time < event.seconds_; });
    };
    const auto first = upper(after);
    return {first, upper(through)};
}
void EventRecorder::Begin(EventTrack base, EventSource source, std::uint64_t generation,
                          double seconds) {
    if (Active()) throw std::logic_error("event.recording_active");
    if (!ValidEvent({seconds, source, 1, generation}))
        throw std::invalid_argument("event.recording_source");
    std::vector<RecordedEvent> captured;
    captured.reserve(EventTrack::kMaximumEvents - base.Events().size());
    const auto last_id = base.LastId();
    base_ = std::move(base);
    captured_ = std::move(captured);
    source_ = source;
    generation_ = generation;
    last_sequence_ = 0;
    last_record_id_ = last_id;
    seconds_ = seconds;
}
RecordingAdmission EventRecorder::Capture(const Event& event) {
    if (!Active()) return RecordingAdmission::kNotRecording;
    if (!ValidEvent(event)) return RecordingAdmission::kInvalid;
    if (event.source_ != *source_) return RecordingAdmission::kWrongSource;
    if (event.generation_ != generation_) return RecordingAdmission::kWrongGeneration;
    if (event.sequence_ <= last_sequence_) return RecordingAdmission::kDuplicateOrOld;
    if (event.seconds_ < seconds_) return RecordingAdmission::kTimeMovedBack;
    if (base_.Events().size() + captured_.size() == EventTrack::kMaximumEvents ||
        last_record_id_ == std::numeric_limits<std::uint64_t>::max())
        return RecordingAdmission::kFull;
    captured_.push_back({++last_record_id_, event.seconds_, event.kind_, event.value_});
    last_sequence_ = event.sequence_;
    seconds_ = event.seconds_;
    return RecordingAdmission::kRecorded;
}
EventTrack EventRecorder::Finish() {
    if (!Active()) throw std::logic_error("event.recording_inactive");
    std::vector<RecordedEvent> combined(base_.Events().begin(), base_.Events().end());
    combined.insert(combined.end(), captured_.begin(), captured_.end());
    EventTrack result(std::move(combined), last_record_id_);
    Cancel();
    return result;
}
void EventRecorder::Cancel() {
    base_ = {};
    captured_.clear();
    source_.reset();
}
}  // namespace rhythm::parameters
