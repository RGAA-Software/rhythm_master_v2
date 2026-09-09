#include "rhythm/player_audio/scene_audio_bridge.h"

#include <stdexcept>

#include "rhythm/audio/playback.h"

namespace rhythm::player_audio {
namespace {
player::SceneAudioSample Translate(std::uint64_t scene_id,
                                   const audio::PlaybackSnapshot& snapshot) {
    using audio::AudioTransitionState;
    using player::SceneAudioPhase;
    const auto& transition = snapshot.transition_;
    player::SceneAudioSample sample;
    sample.transition_id_ = scene_id;
    sample.paused_ = snapshot.paused_ || snapshot.state_ != audio::PlaybackState::kPlaying;
    sample.elapsed_seconds_ = double(transition.elapsed_frames_) / 48000;
    sample.error_ = transition.error_;
    if (transition.previous_presented_)
        sample.previous_ = player::SceneAudioPosition{transition.previous_seconds_,
                                                      transition.previous_iteration_};
    if (transition.incoming_presented_)
        sample.incoming_ = player::SceneAudioPosition{transition.incoming_seconds_,
                                                      transition.incoming_iteration_};
    switch (transition.state_) {
        case AudioTransitionState::kPreparing:
        case AudioTransitionState::kQueued:
            sample.phase_ = SceneAudioPhase::kWaiting;
            break;
        case AudioTransitionState::kMixing:
            sample.phase_ = SceneAudioPhase::kRunning;
            break;
        case AudioTransitionState::kRecovering:
            sample.phase_ = SceneAudioPhase::kRecovering;
            break;
        case AudioTransitionState::kCanceled:
            sample.phase_ = SceneAudioPhase::kCanceled;
            break;
        case AudioTransitionState::kFailed:
            sample.phase_ = SceneAudioPhase::kFailed;
            break;
        case AudioTransitionState::kCompleted:
            sample.phase_ =
                    sample.incoming_ ? SceneAudioPhase::kCommitted : SceneAudioPhase::kFailed;
            if (!sample.incoming_) sample.error_ = "audio.transition_unconfirmed_position";
            break;
        default:
            sample.phase_ = SceneAudioPhase::kFailed;
            sample.error_ = "audio.transition_lost";
            break;
    }
    return sample;
}
}  // namespace
std::optional<player::SceneAudioSample> SceneAudioBridge::Poll(
        const player::SceneDeck& deck, const audio::PlaybackSnapshot& snapshot,
        const BeginSoundtrack& begin, const CancelSoundtrack& cancel) {
    using player::SceneAudioPhase;
    if (!begin || !cancel) throw std::invalid_argument("player.audio_host_callbacks");
    const auto pending = deck.AudioPendingId();
    if (scene_id_) {
        if ((pending != scene_id_ || !deck.Transitioning()) && !cancel_requested_) {
            (void)cancel(audio_id_);
            cancel_requested_ = true;
        }
        if (snapshot.transition_.id_ != audio_id_) {
            const auto rejected_id = scene_id_;
            scene_id_ = audio_id_ = 0;
            cancel_requested_ = false;
            if (pending == rejected_id)
                return player::SceneAudioSample{pending, SceneAudioPhase::kFailed,     false, 0, {},
                                                {},      "audio.transition_superseded"};
        } else {
            const auto sample = Translate(scene_id_, snapshot);
            if (pending == scene_id_) return sample;
            if (audio::AudioTransitionActive(snapshot.transition_.state_)) return {};
            scene_id_ = audio_id_ = 0;
            cancel_requested_ = false;
        }
    }
    if (!pending) return {};
    if (!deck.Transitioning()) return player::SceneAudioSample{pending, SceneAudioPhase::kCanceled};
    if (!deck.AudioReady()) return player::SceneAudioSample{pending, SceneAudioPhase::kWaiting};
    const auto source = deck.IncomingSoundtrack();
    if (!source)
        return player::SceneAudioSample{pending,
                                        SceneAudioPhase::kFailed,
                                        false,
                                        0,
                                        {},
                                        {},
                                        "audio.transition_soundtrack_missing"};
    try {
        const auto id = begin(*source, deck.TransitionDuration(), deck.Current().Paused());
        if (!id) throw std::runtime_error("audio.transition_identity");
        scene_id_ = pending;
        audio_id_ = id;
        cancel_requested_ = false;
        return player::SceneAudioSample{pending, SceneAudioPhase::kWaiting};
    } catch (const std::exception& error) {
        return player::SceneAudioSample{pending,     SceneAudioPhase::kFailed, false, 0, {}, {},
                                        error.what()};
    }
}
}  // namespace rhythm::player_audio
