#pragma once

#include <optional>

#include "rhythm/parameters/events.h"

namespace rhythm::parameters {
// The owning event-input node is the target. Copying/remapping that node carries
// its actions with it, so records never retain detached graph object addresses.
struct RecordedEvent {
    std::uint64_t id_ = 0;
    double seconds_ = 0;
    EventKind kind_ = EventKind::kPulse;
    double value_ = 1;
    bool operator==(const RecordedEvent&) const = default;
};
class EventTrack final {
   public:
    static constexpr std::size_t kMaximumEvents = 4096;
    EventTrack() = default;
    explicit EventTrack(std::vector<RecordedEvent> events, std::uint64_t last_id = 0);
    std::span<const RecordedEvent> Events() const { return events_; }
    // Persisted allocation watermark, including deleted actions. A new action
    // must not inherit a retired action's extension fields after save/reopen.
    std::uint64_t LastId() const { return last_id_; }
    // Half-open traversal (after, through], independent of display frame rate.
    std::span<const RecordedEvent> Between(double after, double through) const;
    bool operator==(const EventTrack&) const = default;

   private:
    std::vector<RecordedEvent> events_{};
    std::uint64_t last_id_ = 0;
};
enum class RecordingAdmission {
    kRecorded,
    kNotRecording,
    kInvalid,
    kWrongSource,
    kWrongGeneration,
    kDuplicateOrOld,
    kTimeMovedBack,
    kFull
};
// Host-thread take buffer. Capture only events actually dispatched by the live
// source. Finish returns one value for a single undoable authoring transaction;
// Cancel discards the take. Existing actions are immutable while recording.
class EventRecorder final {
   public:
    void Begin(EventTrack base, EventSource source, std::uint64_t generation, double seconds);
    RecordingAdmission Capture(const Event& event);
    EventTrack Finish();
    void Cancel();
    bool Active() const { return source_.has_value(); }
    std::size_t Captured() const { return captured_.size(); }

   private:
    EventTrack base_{};
    std::vector<RecordedEvent> captured_{};
    std::optional<EventSource> source_{};
    std::uint64_t generation_ = 0;
    std::uint64_t last_sequence_ = 0;
    std::uint64_t last_record_id_ = 0;
    double seconds_ = 0;
};
}  // namespace rhythm::parameters
