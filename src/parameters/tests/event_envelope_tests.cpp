#include <cmath>
#include <iostream>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <string>

#include "rhythm/parameters/event_envelope.h"

namespace {
void Check(bool value, std::source_location location = std::source_location::current()) {
    if (!value)
        throw std::runtime_error("event.envelope_contract:" + std::to_string(location.line()));
}
void Near(double actual, double expected) { Check(std::abs(actual - expected) < 1e-9); }
}  // namespace
int main() {
    using namespace rhythm::parameters;
    try {
        EventEnvelope envelope({0.1, 0.2, 0.4, 0.5, 1});
        envelope.Reset(1);
        Near(envelope.Sample(0), 0);
        Event event{1, {0, 1, EventOrigin::kManual}, 1, 1, EventKind::kPulse, 1};
        envelope.Apply(event);
        Near(envelope.Sample(1), 0);
        Near(envelope.Sample(1.05), 0.5);
        Near(envelope.Sample(1.1), 1);
        Near(envelope.Sample(1.2), 0.7);
        Near(envelope.Sample(1.3), 0.4);
        Near(envelope.Sample(2), 0.4);
        Near(envelope.Sample(2.25), 0.2);
        Near(envelope.Sample(2.5), 0);
        // Sampling does not advance the envelope clock or mutate its state.
        Near(envelope.Sample(1.05), 0.5);
        event.seconds_ = 3;
        event.kind_ = EventKind::kGate;
        envelope.Apply(event);
        Near(envelope.Sample(5), 0.4);
        event.seconds_ = 5;
        event.value_ = 0;
        envelope.Apply(event);
        Near(envelope.Sample(5.25), 0.2);
        event.seconds_ = 5.1;
        envelope.Apply(event);  // Repeated gate-off must not lengthen release.
        Near(envelope.Sample(5.25), 0.2);
        event.seconds_ = 6;
        event.value_ = 1;
        envelope.Apply(event);
        event.seconds_ = 6.05;
        event.value_ = 0;
        envelope.Apply(event);
        Near(envelope.Sample(6.05), 0.5);
        Near(envelope.Sample(6.3), 0.25);
        event.seconds_ = 6.35;
        event.kind_ = EventKind::kReset;
        event.value_ = 1;
        envelope.Apply(event);
        Near(envelope.Sample(7), 0);

        EventEnvelope instant({0, 0, 1, 0, 0.25});
        instant.Reset(1);
        event.seconds_ = 0;
        event.kind_ = EventKind::kPulse;
        event.value_ = 2;
        instant.Apply(event);
        Near(instant.Sample(0), 2);
        Near(instant.Sample(0.25), 0);
        EventEnvelope early_release({1, 0, 1, 0.5, 0.25});
        early_release.Reset(1);
        early_release.Apply(event);
        Near(early_release.Sample(0.25), 0.5);
        Near(early_release.Sample(0.5), 0.25);
        EventEnvelope zero({0, 0, 0, 0, 0});
        zero.Reset(1);
        zero.Apply(event);
        Near(zero.Sample(0), 0);

        for (int fps : {30, 60, 144}) {
            EventEnvelope sampled({0.1, 0.2, 0.4, 0.5, 1});
            sampled.Reset(1);
            event.seconds_ = 1;
            event.value_ = 1;
            sampled.Apply(event);
            for (int frame = 0; frame < fps * 2; ++frame) sampled.Sample(1 + double(frame) / fps);
            Near(sampled.Sample(1.05), 0.5);
            Near(sampled.Sample(1.2), 0.7);
            Near(sampled.Sample(2.25), 0.2);
        }
        const auto before = instant.Sample(0.1);
        for (int invalid = 0; invalid < 3; ++invalid) {
            auto bad = event;
            if (invalid == 0) bad.generation_ = 2;
            if (invalid == 1) bad.seconds_ = -1;
            if (invalid == 2) bad.value_ = std::numeric_limits<double>::infinity();
            try {
                instant.Apply(bad);
                Check(false);
            } catch (const std::invalid_argument&) {
            }
            Near(instant.Sample(0.1), before);
        }
        instant.Reset(2, 0.1);
        Near(instant.Sample(0.1), 0);
        Check(!ValidEventEnvelopeSettings({-1, 0, 0, 0, 0}));
        Check(!ValidEventEnvelopeSettings({0, 0, 2, 0, 0}));
        std::cout << "Timestamped pulse/gate/reset ADSR, early release and refresh-rate "
                     "independence passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
