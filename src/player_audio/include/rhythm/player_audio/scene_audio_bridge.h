#pragma once

#include <functional>

#include "rhythm/player/scene_deck.h"

namespace rhythm::audio {
struct PlaybackSnapshot;
}
namespace rhythm::player_audio {
using BeginSoundtrack = std::function<std::uint64_t(const media::SoundtrackSource&, double, bool)>;
using CancelSoundtrack = std::function<bool(std::uint64_t)>;
// Host-thread request binding. The callbacks only enqueue audio commands; they
// perform no decode or device I/O. Poll receives the same snapshot used for
// this frame's time/FFT, so metadata cannot come from a different worker instant.
class SceneAudioBridge final {
   public:
    std::optional<player::SceneAudioSample> Poll(const player::SceneDeck& deck,
                                                 const audio::PlaybackSnapshot& snapshot,
                                                 const BeginSoundtrack& begin,
                                                 const CancelSoundtrack& cancel);

   private:
    std::uint64_t scene_id_ = 0;
    std::uint64_t audio_id_ = 0;
    bool cancel_requested_ = false;
};
}  // namespace rhythm::player_audio
