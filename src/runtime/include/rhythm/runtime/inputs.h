#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <optional>

#include "rhythm/audio/features.h"

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
    bool operator==(const ExternalInputs&) const = default;
};
inline bool ValidExternalInputs(const ExternalInputs& inputs) {
    return ValidInputs(inputs.participant_) &&
           (!inputs.session_seconds_ ||
            (std::isfinite(*inputs.session_seconds_) && *inputs.session_seconds_ >= 0)) &&
           (!inputs.audio_ || audio::ValidFeatures(*inputs.audio_));
}
}  // namespace rhythm::runtime
