#pragma once

#include <optional>

#include "rhythm/parameters/controls.h"

namespace rhythm::parameters {
struct ControlCue {
    std::uint64_t id_ = 0;
    std::string title_{};
    double seconds_ = 0;
    std::uint64_t snapshot_ = 0;
    double fade_ = 0;
    bool smooth_ = false;
    bool operator==(const ControlCue&) const = default;
};
// Immutable, bounded cue arrangement. The caller supplies the unified work time.
// Interrupted transitions start from the value at the interruption, so random
// seeks and offline frames give the same result as continuous playback.
class ControlSequence final {
   public:
    ControlSequence() = default;
    ControlSequence(const ControlBank& bank, std::vector<ControlCue> cues);
    std::span<const ControlCue> Cues() const { return cues_; }
    ControlValues Sample(double seconds) const;
    std::optional<std::uint64_t> Active(double seconds) const;
    // Forward crossings use (from, to]; reverse seeks emit no trigger events.
    // Hosts suppress crossing delivery during seeks/restarts, and may query the
    // active cue at zero separately. Values never depend on event delivery.
    std::vector<std::uint64_t> Crossed(double from, double to) const;
    bool operator==(const ControlSequence&) const = default;
    static constexpr std::size_t kMaximumCues = 256;

   private:
    struct Transition {
        ControlValues from_{};
        ControlValues to_{};
        bool operator==(const Transition&) const = default;
    };
    std::vector<ControlCue> cues_{};
    std::vector<Transition> transitions_{};
    ControlValues defaults_{};
};
// Explicit host overrides win per control. Unmodified controls follow the cue
// sequence; clearing overrides returns to automation at the supplied time.
ControlValues EvaluateControls(const ControlBank& bank,
                               const std::optional<ControlSequence>& sequence, double seconds,
                               const ControlValues& overrides = {});
}  // namespace rhythm::parameters
