#include "rhythm/parameters/events.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <tuple>

namespace rhythm::parameters {
namespace {
bool Time(double seconds) { return std::isfinite(seconds) && seconds >= 0 && seconds <= 1e9; }
}  // namespace
bool ValidEvent(const Event& event) {
    return Time(event.seconds_) && event.source_.node_ && event.sequence_ && event.generation_ &&
           event.source_.origin_ <= EventOrigin::kRecorded && event.kind_ <= EventKind::kReset &&
           std::isfinite(event.value_) && std::abs(event.value_) <= 1e6 &&
           (event.kind_ != EventKind::kGate || event.value_ == 0 || event.value_ == 1) &&
           (event.kind_ != EventKind::kReset || event.value_ == 1);
}
bool EventBefore(const Event& first, const Event& second) {
    // Existing track actions precede a newly performed action at the same node
    // and time. Appending a take then preserves that order when all are replayed.
    const auto key = [](const Event& event) {
        const int origin = event.source_.origin_ == EventOrigin::kRecorded
                                   ? -1
                                   : static_cast<int>(event.source_.origin_);
        return std::tuple(event.seconds_, event.source_.scope_, event.source_.node_, origin,
                          event.sequence_);
    };
    return key(first) < key(second);
}
bool EventBatch::Append(const Event& event) {
    if (count_ == kCapacity || !ValidEvent(event)) return false;
    events_[count_++] = event;
    return true;
}
EventQueue::EventQueue() { pending_.reserve(kCapacity); }
void EventQueue::Reset(std::uint64_t generation, double seconds) {
    if (!generation || !Time(seconds)) throw std::invalid_argument("event.reset");
    pending_.clear();
    source_count_ = 0;
    generation_ = generation;
    seconds_ = seconds;
}
EventAdmission EventQueue::Submit(const Event& event) {
    if (!ValidEvent(event)) return EventAdmission::kInvalid;
    if (event.generation_ != generation_) return EventAdmission::kWrongGeneration;
    if (event.seconds_ < seconds_) return EventAdmission::kLate;
    const auto end = sources_.begin() + source_count_;
    const auto source = std::find_if(sources_.begin(), end, [&](const auto& value) {
        return value.source_ == event.source_;
    });
    if (source != end) {
        if (event.sequence_ <= source->sequence_) return EventAdmission::kDuplicateOrOld;
        if (event.seconds_ < source->seconds_) return EventAdmission::kLate;
    } else if (source_count_ == kSourceCapacity) {
        return EventAdmission::kSourceLimit;
    }
    if (pending_.size() == kCapacity) return EventAdmission::kQueueFull;
    // Admission checks finish before committing either queue or watermark.
    pending_.insert(std::upper_bound(pending_.begin(), pending_.end(), event, EventBefore), event);
    *source = {event.source_, event.sequence_, event.seconds_};
    if (source == end) ++source_count_;
    return EventAdmission::kAccepted;
}
EventDispatch EventQueue::Drain(double seconds, bool paused) {
    if (!Time(seconds) || seconds < seconds_) throw std::invalid_argument("event.time");
    seconds_ = seconds;
    EventDispatch result;
    const auto end =
            std::upper_bound(pending_.begin(), pending_.end(), seconds,
                             [](double time, const Event& event) { return time < event.seconds_; });
    const auto due = static_cast<std::size_t>(end - pending_.begin());
    const auto count = paused ? 0 : std::min(due, EventBatch::kCapacity);
    for (std::size_t index = 0; index < count; ++index) result.batch_.Append(pending_[index]);
    pending_.erase(pending_.begin(), pending_.begin() + count);
    result.due_remaining_ = due - count;
    return result;
}
}  // namespace rhythm::parameters
