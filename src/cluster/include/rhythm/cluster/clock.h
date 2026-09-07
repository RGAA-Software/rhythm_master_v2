#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace rhythm::cluster {
struct ClockProbe {
    std::uint64_t epoch_ = 0;
    std::uint64_t sequence_ = 0;
    std::int64_t local_send_us_ = 0;
    std::int64_t host_receive_us_ = 0;
    std::int64_t host_send_us_ = 0;
    std::int64_t local_receive_us_ = 0;
};
enum class ProbeResult { kAccepted, kInvalid, kOldEpoch, kReplay, kDelayed, kResyncRequired };
struct ClockEstimate {
    bool ready_ = false;
    bool resync_required_ = false;
    double host_us_ = 0;
    double drift_ppm_ = 0;
    double uncertainty_us_ = 0;
};
// Owner-thread only. Feed authenticated, request-matched four-timestamp probes.
// This estimates a session monotonic clock; it never changes the system clock.
// Advance slews small corrections. Discontinuities require explicit Reset and
// fresh probes before a host can schedule presentation again.
class ClockSynchronizer final {
   public:
    explicit ClockSynchronizer(std::uint64_t epoch);
    void Reset(std::uint64_t epoch);
    ProbeResult Observe(ClockProbe probe);
    ClockEstimate Estimate(std::int64_t local_us) const;
    std::optional<std::int64_t> Advance(std::int64_t local_us);

   private:
    struct Sample {
        double local_us_ = 0;
        double offset_us_ = 0;
        double delay_us_ = 0;
    };
    void Fit(double now_us);
    std::array<Sample, 32> samples_{};
    std::size_t count_ = 0;
    std::size_t next_ = 0;
    std::uint64_t epoch_ = 0;
    std::uint64_t sequence_ = 0;
    std::int64_t received_us_ = 0;
    double reference_us_ = 0;
    double offset_us_ = 0;
    double drift_ppm_ = 0;
    double uncertainty_us_ = 0;
    bool ready_ = false;
    bool resync_required_ = false;
    std::optional<std::int64_t> last_local_us_{};
    double last_host_us_ = 0;
};
}  // namespace rhythm::cluster
