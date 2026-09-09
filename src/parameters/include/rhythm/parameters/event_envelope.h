#pragma once

#include <optional>

#include "rhythm/parameters/events.h"

namespace rhythm::parameters {
struct EventEnvelopeSettings {
    double attack_ = 0.01;
    double decay_ = 0.1;
    double sustain_ = 0.7;
    double release_ = 0.3;
    // Pulse releases this many seconds after its timestamp; gate waits for off.
    double duration_ = 0.3;
    bool operator==(const EventEnvelopeSettings&) const = default;
};
bool ValidEventEnvelopeSettings(const EventEnvelopeSettings& settings);
// Piecewise-linear ADSR adapted from TiXL's AdsrCalculator. Absolute event time
// preserves phase across refresh rates, rather than accumulating frame deltas.
// Host/evaluation-thread confined; a sampled value owns no device or clock.
class EventEnvelope final {
   public:
    explicit EventEnvelope(EventEnvelopeSettings settings = {});
    void Reset(std::uint64_t generation, double seconds = 0);
    void Apply(const Event& event);
    double Sample(double seconds) const;

   private:
    double Held(double seconds) const;
    EventEnvelopeSettings settings_{};
    std::uint64_t generation_ = 0;
    double last_event_seconds_ = 0;
    std::optional<double> start_{};
    std::optional<double> release_{};
    double release_value_ = 0;
    double amplitude_ = 1;
};
}  // namespace rhythm::parameters
