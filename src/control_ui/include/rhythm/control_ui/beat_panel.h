#pragma once

#include <map>
#include <string>

#include "rhythm/player/performance_actions.h"

namespace rhythm::control_ui {
struct BeatEdit {
    bool changed_ = false;
    std::optional<parameters::BeatSettings> grid_{};
};
// UI-thread tempo gestures only. Hosts own saved settings, clock and requests.
class BeatPanel final {
   public:
    BeatEdit Draw(const std::optional<parameters::BeatSettings>& grid, double seconds,
                  const std::map<std::string, std::string>& text);
    parameters::Quantization Mode() const { return mode_; }
    void Reset() { *this = BeatPanel{}; }

   private:
    parameters::Quantization mode_ = parameters::Quantization::kImmediate;
    parameters::TapTempo taps_{};
};
// Returns true only for the displayed request's cancel button.
bool DrawPerformanceAction(const player::PerformanceAction& action,
                           const std::map<std::string, std::string>& text);
}  // namespace rhythm::control_ui
