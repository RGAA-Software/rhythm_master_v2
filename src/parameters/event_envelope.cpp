// Adapted from TiXL Core/Audio/AdsrCalculator.cs (MIT).
// Copyright (c) 2010-2016 Thomas Mann, Daniel Szymanski, Andreas Rose,
// Framefield GmbH. See third_party/notices/tixl-effects/LICENSE.txt.
#include "rhythm/parameters/event_envelope.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace rhythm::parameters {
namespace {
bool Duration(double value) { return std::isfinite(value) && value >= 0 && value <= 86400; }
bool Time(double value) { return std::isfinite(value) && value >= 0 && value <= 1e9; }
}  // namespace
bool ValidEventEnvelopeSettings(const EventEnvelopeSettings& settings) {
    return Duration(settings.attack_) && Duration(settings.decay_) && Duration(settings.release_) &&
           Duration(settings.duration_) && std::isfinite(settings.sustain_) &&
           settings.sustain_ >= 0 && settings.sustain_ <= 1;
}
EventEnvelope::EventEnvelope(EventEnvelopeSettings settings) : settings_(settings) {
    if (!ValidEventEnvelopeSettings(settings))
        throw std::invalid_argument("event.envelope_settings");
}
void EventEnvelope::Reset(std::uint64_t generation, double seconds) {
    if (!generation || !Time(seconds)) throw std::invalid_argument("event.envelope_reset");
    generation_ = generation;
    last_event_seconds_ = seconds;
    start_.reset();
    release_.reset();
    release_value_ = 0;
    amplitude_ = 1;
}
double EventEnvelope::Held(double seconds) const {
    const auto elapsed = seconds - *start_;
    if (elapsed < settings_.attack_) return elapsed / settings_.attack_;
    if (elapsed < settings_.attack_ + settings_.decay_)
        return std::lerp(1.0, settings_.sustain_, (elapsed - settings_.attack_) / settings_.decay_);
    return settings_.sustain_;
}
double EventEnvelope::Sample(double seconds) const {
    if (!Time(seconds) || seconds < last_event_seconds_)
        throw std::invalid_argument("event.envelope_time");
    if (!start_) return 0;
    if (!release_ || seconds < *release_) return amplitude_ * Held(seconds);
    if (settings_.release_ == 0) return 0;
    return amplitude_ * release_value_ *
           std::clamp(1 - (seconds - *release_) / settings_.release_, 0.0, 1.0);
}
void EventEnvelope::Apply(const Event& event) {
    if (!ValidEvent(event) || event.generation_ != generation_ ||
        event.seconds_ < last_event_seconds_)
        throw std::invalid_argument("event.envelope_input");
    last_event_seconds_ = event.seconds_;
    if (event.kind_ == EventKind::kReset) {
        Reset(generation_, event.seconds_);
        return;
    }
    if (event.kind_ == EventKind::kGate && event.value_ == 0) {
        if (start_ && (!release_ || event.seconds_ < *release_)) {
            release_value_ = Held(event.seconds_);
            release_ = event.seconds_;
        }
        return;
    }
    start_ = event.seconds_;
    amplitude_ = event.value_;
    release_.reset();
    if (event.kind_ == EventKind::kPulse) {
        release_ = event.seconds_ + settings_.duration_;
        release_value_ = Held(*release_);
    }
}
}  // namespace rhythm::parameters
