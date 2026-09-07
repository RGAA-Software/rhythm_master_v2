#include <cmath>
#include <iostream>
#include <source_location>
#include <stdexcept>
#include <string>

#include "rhythm/cluster/clock.h"

namespace {
void Check(bool value, std::source_location location = std::source_location::current()) {
    if (!value) throw std::runtime_error("clock.contract:" + std::to_string(location.line()));
}
rhythm::cluster::ClockProbe Probe(std::uint64_t sequence, std::int64_t local,
                                  double drift_ppm = 100, double offset_us = 2000000) {
    const auto host = [&](std::int64_t time) {
        return static_cast<std::int64_t>(
                std::llround(time * (1 + drift_ppm / 1000000) + offset_us));
    };
    return {7, sequence, local, host(local + 3000), host(local + 3200), local + 6200};
}
}  // namespace
int main() {
    using namespace rhythm::cluster;
    try {
        for (const double drift : {-100.0, 0.0, 100.0}) {
            ClockSynchronizer clock(7);
            Check(!clock.Advance(0));
            std::int64_t previous_host = 0;
            // One hour of synthetic probes, with a finite 32-sample window.
            for (std::uint64_t second = 0; second < 3600; ++second) {
                const auto local = static_cast<std::int64_t>((second + 10) * 1000000);
                const auto probe = Probe(second + 1, local, drift);
                Check(clock.Observe(probe) == ProbeResult::kAccepted);
                const auto estimate = clock.Estimate(local + 10000);
                if (second < 2) {
                    Check(!estimate.ready_);
                    continue;
                }
                const auto expected = (local + 10000) * (1 + drift / 1000000) + 2000000;
                Check(estimate.ready_ && std::abs(estimate.host_us_ - expected) < 2 &&
                      std::abs(estimate.drift_ppm_ - drift) < 1 &&
                      estimate.uncertainty_us_ >= 3000);
                const auto mapped = clock.Advance(local + 10000);
                Check(mapped && *mapped > previous_host && std::abs(*mapped - expected) < 2);
                previous_host = *mapped;
                Check(clock.Observe(probe) == ProbeResult::kReplay);
            }
        }
        ClockSynchronizer clock(7);
        for (std::uint64_t index = 1; index <= 5; ++index)
            Check(clock.Observe(Probe(index, static_cast<std::int64_t>(index * 1000000))) ==
                  ProbeResult::kAccepted);
        auto invalid = Probe(6, 6000000);
        invalid.epoch_ = 8;
        Check(clock.Observe(invalid) == ProbeResult::kOldEpoch);
        invalid = Probe(6, 6000000);
        invalid.local_send_us_ = -1;
        Check(clock.Observe(invalid) == ProbeResult::kInvalid);
        invalid = Probe(6, 6000000);
        invalid.local_receive_us_ += 50000;
        Check(clock.Observe(invalid) == ProbeResult::kDelayed);
        const auto before = clock.Advance(5010000);
        Check(before.has_value());
        // A small correction cannot move playback backwards or cause a step.
        Check(clock.Observe(Probe(7, 5100000, 100, 1990000)) == ProbeResult::kAccepted);
        const auto after = clock.Advance(5110000);
        Check(after && *after > *before && *after - *before < 101000);
        Check(clock.Observe(Probe(8, 5200000, 100, 4000000)) == ProbeResult::kResyncRequired);
        Check(!clock.Advance(5210000) && clock.Estimate(5210000).resync_required_);
        clock.Reset(7);
        Check(clock.Observe(Probe(7, 5100000)) == ProbeResult::kReplay);
        clock.Reset(8);
        Check(clock.Observe(Probe(9, 9000000)) == ProbeResult::kOldEpoch);
        ClockSynchronizer stale(7);
        for (std::uint64_t index = 1; index <= 3; ++index)
            stale.Observe(Probe(index, static_cast<std::int64_t>(index * 1000000)));
        Check(stale.Advance(3010000).has_value());
        Check(!stale.Estimate(10000000).ready_ && !stale.Advance(10000000) &&
              stale.Estimate(10000000).resync_required_);
        std::cout << "clock contracts passed: hour-long drift, delay spikes, replay, epochs, slew, "
                     "resync\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
