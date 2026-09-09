#pragma once

#include <array>
#include <compare>
#include <cstdint>
#include <span>
#include <vector>

namespace rhythm::parameters {
enum class EventOrigin : std::uint8_t { kBeat, kCue, kAudio, kManual, kOperator, kRecorded };
enum class EventKind : std::uint8_t { kPulse, kGate, kReset };
struct EventSource {
    std::uint64_t scope_ = 0;  // Root is zero; component instances use stable graph IDs.
    std::uint64_t node_ = 0;
    EventOrigin origin_ = EventOrigin::kOperator;
    auto operator<=>(const EventSource&) const = default;
};
struct Event {
    double seconds_ = 0;
    EventSource source_{};
    std::uint64_t sequence_ = 0;
    std::uint64_t generation_ = 0;
    EventKind kind_ = EventKind::kPulse;
    double value_ = 1;
    bool operator==(const Event&) const = default;
};
bool ValidEvent(const Event& event);
bool EventBefore(const Event& first, const Event& second);

// Immutable view of one bounded dispatch result. Copies own their event values.
class EventBatch final {
   public:
    static constexpr std::size_t kCapacity = 256;
    std::span<const Event> Events() const { return {events_.data(), count_}; }
    bool Append(const Event& event);

   private:
    std::array<Event, kCapacity> events_{};
    std::size_t count_ = 0;
};
enum class EventAdmission : std::uint8_t {
    kAccepted,
    kInvalid,
    kWrongGeneration,
    kLate,
    kDuplicateOrOld,
    kSourceLimit,
    kQueueFull
};
struct EventDispatch {
    EventBatch batch_{};
    std::size_t due_remaining_ = 0;
};
// Single evaluation-owner inbox; host adapters publish values, never callbacks.
// Drain once per host frame, then share its immutable batch with all consumers.
// Per-source sequence/time are monotonic. Old IDs remain rejected after dispatch
// without an unbounded deduplication history. Reset is explicit on seek/loop/load.
class EventQueue final {
   public:
    static constexpr std::size_t kCapacity = 1024;
    static constexpr std::size_t kSourceCapacity = 128;
    EventQueue();
    void Reset(std::uint64_t generation, double seconds);
    EventAdmission Submit(const Event& event);
    EventDispatch Drain(double seconds, bool paused = false);
    std::size_t Pending() const { return pending_.size(); }

   private:
    struct Watermark {
        EventSource source_{};
        std::uint64_t sequence_ = 0;
        double seconds_ = 0;
    };
    std::vector<Event> pending_{};
    std::array<Watermark, kSourceCapacity> sources_{};
    std::size_t source_count_ = 0;
    std::uint64_t generation_ = 0;
    double seconds_ = 0;
};
}  // namespace rhythm::parameters
