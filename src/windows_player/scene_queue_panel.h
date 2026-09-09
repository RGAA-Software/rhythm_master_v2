#pragma once

#include <map>

#include "rhythm/player/scene_deck.h"
#include "rhythm/player/scene_queue.h"

namespace rhythm::player_ui {
struct SceneChoice {
    std::filesystem::path package_{};
    std::map<std::string, std::string> titles_{};
    performance::WorkReference reference_{};
};
// UI gestures only. The host supplies the startup catalog and owns the queue,
// deck and audio handoff. No disk access or waiting occurs in Draw.
class SceneQueuePanel final {
   public:
    void Draw(player::SceneQueue& queue, player::SceneDeck& deck,
              std::span<const SceneChoice> choices, const std::string& locale,
              const std::map<std::string, std::string>& text,
              parameters::Quantization mode = parameters::Quantization::kImmediate);

   private:
    std::size_t choice_ = 0;
    std::uint64_t selected_ = 0;
    float duration_ = 1;
    bool full_ = false;
    bool visible_ = false;
};
}  // namespace rhythm::player_ui
