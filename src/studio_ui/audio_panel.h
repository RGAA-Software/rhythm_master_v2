#pragma once

#include <map>
#include <optional>
#include <string>

#include "rhythm/audio/capture.h"

namespace rhythm::studio {
class AudioPanel final {
   public:
    void Draw(const std::map<std::string, std::string>& text);
    std::optional<audio::Features> Snapshot() const;
    void SetSuspended(bool suspended);

   private:
    audio::SystemCapture capture_{};
    bool suspended_ = false;
    bool resume_capture_ = false;
};
}  // namespace rhythm::studio
