#include "metrics.h"

#ifdef _WIN32
#include <windows.h>
// PSAPI declarations require the Windows types above; preserve this order.
#include <psapi.h>
#else
#include <sys/resource.h>
#endif

#include <stdexcept>

namespace rhythm::quic_probe {
Metrics Measure() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS memory{};
    memory.cb = sizeof(memory);
    FILETIME created{}, exited{}, kernel{}, user{};
    // Borrowed process pseudo-handle is used only in these synchronous OS calls.
    if (!GetProcessMemoryInfo(GetCurrentProcess(), &memory, sizeof(memory)) ||
        !GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user))
        throw std::runtime_error("probe.process_metrics");
    const auto time = [](FILETIME value) {
        return (std::uint64_t{value.dwHighDateTime} << 32) | value.dwLowDateTime;
    };
    return {static_cast<double>(time(kernel) + time(user)) / 10000,
            static_cast<std::uint64_t>(memory.PeakWorkingSetSize)};
#else
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0) throw std::runtime_error("probe.process_metrics");
    return {static_cast<double>(usage.ru_utime.tv_sec + usage.ru_stime.tv_sec) * 1000 +
                    static_cast<double>(usage.ru_utime.tv_usec + usage.ru_stime.tv_usec) / 1000,
            static_cast<std::uint64_t>(usage.ru_maxrss) * 1024};
#endif
}
}  // namespace rhythm::quic_probe
