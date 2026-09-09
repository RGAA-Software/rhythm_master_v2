#pragma once

#include <array>

#include "rhythm/player/performance_program.h"
#include "scene_queue_panel.h"

namespace rhythm::player_ui {
// Editing gestures and status only. Blocking work remains in PerformanceProgram.
class PerformancePanel final {
   public:
    void Draw(player::PerformanceProgram& program, std::span<const SceneChoice> choices,
              const std::string& locale, const std::map<std::string, std::string>& text);

   private:
    bool visible_ = false;
    std::size_t choice_ = 0;
    std::uint64_t selected_ = 0;
    float duration_ = 1;
    int quantization_ = 0;
    std::array<char, 4096> import_path_{};
};
}  // namespace rhythm::player_ui
