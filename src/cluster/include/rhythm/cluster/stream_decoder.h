#pragma once

#include "rhythm/cluster/send_queue.h"

namespace rhythm::cluster {
enum class StreamFeedStatus { kConsumed, kBackpressure, kInvalid, kClosed };
struct StreamFeedResult {
    StreamFeedStatus status_ = StreamFeedStatus::kConsumed;
    std::size_t consumed_ = 0;
};
// Serialized-access decoder for one ordered control or resource stream; no
// internal locks. Callers serialize Feed/Take/Finish/Cancel. Wire framing
// is a four-byte big-endian nonzero payload length followed by exactly that many
// bytes. Channel/connection identity is supplied by the authenticated adapter.
// Backpressure retains a bounded partial header/body; the adapter retains only
// the unconsumed suffix, using native receive flow control rather than copying it.
class StreamDecoder final {
   public:
    explicit StreamDecoder(SendChannel channel);
    StreamFeedResult Feed(std::span<const std::uint8_t> bytes);
    std::optional<SendPayload> Take();
    // Call only after all bytes preceding transport FIN have been consumed.
    // Already queued complete messages remain available after a clean Finish.
    bool Finish();
    void Cancel();
    std::size_t QueuedBytes() const { return queued_bytes_; }
    std::size_t QueuedCount() const { return ready_.size(); }

   private:
    std::deque<SendPayload> ready_{};
    std::array<std::uint8_t, 4> header_{};
    std::vector<std::uint8_t> body_{};
    std::size_t header_used_ = 0;
    std::size_t body_used_ = 0;
    std::size_t expected_ = 0;
    std::size_t queued_bytes_ = 0;
    std::size_t maximum_message_ = 0;
    std::size_t maximum_bytes_ = 0;
    std::size_t maximum_count_ = 0;
    bool closed_ = false;
};
}  // namespace rhythm::cluster
