#pragma once

#include <array>
#include <optional>

#include "rhythm/cluster/clock.h"
#include "rhythm/cluster/datagram_codec.h"

namespace rhythm::cluster {
// Host-thread request matching after connection authentication. Eight outstanding
// probes, one-second expiry, no allocation. Local receive times come from the
// caller's monotonic clock. Resetting the same epoch cannot reuse request IDs.
class ClockExchange final {
   public:
    explicit ClockExchange(std::uint64_t epoch);
    void Reset(std::uint64_t epoch);
    std::optional<ClockRequest> Begin(std::int64_t local_now_us);
    std::optional<ClockProbe> Complete(const ClockReply& reply, std::int64_t local_now_us);

   private:
    bool Advance(std::int64_t local_now_us);
    std::array<std::optional<ClockRequest>, 8> pending_{};
    std::uint64_t epoch_ = 0;
    std::uint64_t sequence_ = 0;
    std::optional<std::int64_t> last_local_us_{};
};
}  // namespace rhythm::cluster
