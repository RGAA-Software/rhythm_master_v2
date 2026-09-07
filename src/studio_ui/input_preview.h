#pragma once

#include <map>
#include <string>

#include "rhythm/runtime/inputs.h"

namespace rhythm::studio {
// Transient authoring simulation; no room membership or transport is created.
class InputPreview final {
   public:
    void Draw(const std::map<std::string, std::string>& text);
    runtime::ExternalInputs Snapshot(double local_seconds) const;

   private:
    runtime::ParticipantInputs values_{};
    bool enabled_ = false;
    bool session_clock_ = false;
    double offset_seconds_ = 0;
    int channel_ = 0;
};
}  // namespace rhythm::studio
