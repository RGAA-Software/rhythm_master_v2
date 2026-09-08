#pragma once

#include <array>
#include <optional>

#include "rhythm/parameters/controls.h"

namespace rhythm::control_ui {
struct Edit {
    // Sliders publish only changed IDs; snapshot recall/blend publishes all IDs.
    // Hosts merge these overrides, leaving other controls on cue automation.
    std::optional<parameters::ControlValues> values_{};
    bool committed_ = false;
    std::string capture_{};
    std::uint64_t remove_ = 0;
};
// UI-thread gesture state only. Hosts own values and commit/undo policy.
class ControlPanel final {
   public:
    Edit Draw(const parameters::ControlBank& bank, const parameters::ControlValues& current,
              const std::map<std::string, std::string>& text, bool authoring = false);
    void Reset() { *this = ControlPanel{}; }

   private:
    std::uint64_t first_ = 0;
    std::uint64_t second_ = 0;
    float blend_ = 0;
    std::array<char, 129> name_{};
};
}  // namespace rhythm::control_ui
