#pragma once

#include <cstdint>

namespace rhythm::quic_probe {
struct Metrics {
    double cpu_ms_ = 0;
    std::uint64_t peak_resident_bytes_ = 0;
};
Metrics Measure();
}  // namespace rhythm::quic_probe
