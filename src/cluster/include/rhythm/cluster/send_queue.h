#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace rhythm::cluster {
// Owns immutable bytes shared across peers before per-connection encryption.
// At most 64 KiB; public construction cannot retain a mutable vector owner.
class SendPayload final {
   public:
    SendPayload() = default;
    explicit SendPayload(std::span<const std::uint8_t> bytes);
    std::span<const std::uint8_t> Bytes() const;

   private:
    std::shared_ptr<const std::vector<std::uint8_t>> bytes_{};
};
enum class SendChannel { kControl, kRealtime, kAsset };
enum class QueueResult { kAccepted, kReplaced, kFull, kClosed, kInvalid };
struct SendTicket {
    std::uint64_t generation_ = 0;
    std::uint64_t id_ = 0;
    bool operator==(const SendTicket&) const = default;
};
struct PendingSend {
    SendTicket ticket_{};
    SendChannel channel_ = SendChannel::kControl;
    SendPayload payload_{};
};
struct SendQueueStats {
    std::size_t pending_bytes_ = 0;
    std::size_t in_flight_bytes_ = 0;
    std::size_t high_water_bytes_ = 0;
    std::size_t in_flight_count_ = 0;
    std::uint64_t replaced_realtime_ = 0;
};
// Owner-thread lifecycle: Enqueue -> Acquire -> transport completion -> Complete.
// Acquire retains bytes and reserves a slot until completion. Close drops pending
// work but retains in-flight ownership; the adapter must quiesce native callbacks
// before destruction. It must not retain completed PendingSend values indefinitely.
// Logical per-peer budgets include in-flight bytes, even for shared payloads.
// Reset requires a strictly increasing local connection generation.
class SendQueue final {
   public:
    explicit SendQueue(std::uint64_t generation);
    SendQueue(const SendQueue&) = delete;
    SendQueue& operator=(const SendQueue&) = delete;
    SendQueue(SendQueue&&) = delete;
    SendQueue& operator=(SendQueue&&) = delete;
    QueueResult Enqueue(SendChannel channel, SendPayload payload);
    std::optional<PendingSend> Acquire();
    bool Complete(SendTicket ticket);
    void Close();
    void Reset(std::uint64_t generation);
    SendQueueStats Stats() const;

   private:
    std::array<std::optional<PendingSend>, 16> in_flight_{};
    std::deque<PendingSend> control_{};
    std::deque<PendingSend> assets_{};
    std::optional<PendingSend> realtime_{};
    SendQueueStats stats_{};
    std::uint64_t generation_ = 0;
    std::uint64_t next_id_ = 0;
    bool closed_ = false;
};
}  // namespace rhythm::cluster
