#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>

#include "rhythm/audio/features.h"
#include "rhythm/parameters/controls.h"
#include "rhythm/parameters/events.h"

namespace rhythm::runtime {
struct ParticipantInputs {
    std::uint32_t group_ = 0;
    std::uint32_t index_ = 0;
    std::array<double, 32> controls_{};
    bool operator==(const ParticipantInputs&) const = default;
};
inline bool ValidInputs(const ParticipantInputs& inputs) {
    if (inputs.group_ > 65535 || inputs.index_ > 65535) return false;
    for (const auto value : inputs.controls_)
        if (!std::isfinite(value) || value < 0 || value > 1) return false;
    return true;
}
// Immutable values for one evaluation. Missing session time selects the local
// clock. Normalized controls are explicit channels, not network endpoints or
// a substitute for the separately specified canonical audio-feature contract.
struct ExternalInputs {
    std::optional<double> session_seconds_{};
    ParticipantInputs participant_{};
    std::optional<audio::Features> audio_{};
    parameters::ControlValues controls_{};
    // Host-owned queue dispatch for this evaluation; targets are stable root
    // event.input IDs. Sharing is immutable and never carries host callbacks.
    std::shared_ptr<const parameters::EventBatch> events_{};
    bool operator==(const ExternalInputs&) const = default;
};
inline bool ValidExternalInputs(const ExternalInputs& inputs) {
    if (inputs.events_)
        for (const auto& event : inputs.events_->Events())
            if (!parameters::ValidEvent(event) || event.source_.scope_ != 0 ||
                event.source_.origin_ != parameters::EventOrigin::kManual)
                return false;
    if (inputs.controls_.size() > parameters::ControlBank::kMaximumControls) return false;
    for (const auto& [id, value] : inputs.controls_)
        if (!id || !std::isfinite(value) || std::abs(value) > 1e6) return false;
    return ValidInputs(inputs.participant_) &&
           (!inputs.session_seconds_ ||
            (std::isfinite(*inputs.session_seconds_) && *inputs.session_seconds_ >= 0)) &&
           (!inputs.audio_ || audio::ValidFeatures(*inputs.audio_));
}
}  // namespace rhythm::runtime
