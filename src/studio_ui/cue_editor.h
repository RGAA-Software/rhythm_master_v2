#pragma once

#include "rhythm/graph/document.h"

namespace rhythm::studio {
struct CueEdit {
    std::optional<std::vector<parameters::ControlCue>> cues_{};
    bool committed_ = false;
};
// Timeline widget state only. The caller owns preview and undo transactions.
class CueEditor final {
   public:
    CueEdit Draw(const graph::Document& document, double playhead, double duration, double bpm,
                 const std::map<std::string, std::string>& text);
    void Reset() { *this = CueEditor{}; }

   private:
    std::uint64_t selected_ = 0;
    std::uint64_t snapshot_ = 0;
    std::uint64_t dragged_ = 0;
    double anchor_ = 0;
    float mouse_anchor_ = 0;
    bool snap_ = true;
    std::string error_{};
};
}  // namespace rhythm::studio
