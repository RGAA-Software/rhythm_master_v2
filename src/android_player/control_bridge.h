#pragma once

#include "rhythm/player/scene_deck.h"

namespace rhythm::android_host {
// Render-thread publication and snapshot read. JNI edits are validated under
// the adapter mutex; no Java object or render resource crosses the boundary.
void PublishControls(const player::SceneDeck& deck);
void PublishControlFrame(const player::SceneDeck& deck);
void ApplyControlCommands(player::SceneDeck& deck);
parameters::Quantization CurrentQuantization();
}  // namespace rhythm::android_host
