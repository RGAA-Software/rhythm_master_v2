#pragma once

#include "rhythm/player/scene_deck.h"
#include "rhythm/player/scene_queue.h"

namespace rhythm::android_host {
struct SceneCommands {
    std::string path_{};
    std::string title_{};
    int action_ = 0;
    std::uint64_t id_ = 0;
    double duration_ = 1;
};
SceneCommands TakeSceneCommands();
void PublishSceneQueue(const player::SceneQueue& queue, const player::SceneDeck& deck);
}  // namespace rhythm::android_host
