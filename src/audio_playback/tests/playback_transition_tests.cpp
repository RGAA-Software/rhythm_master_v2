#include <chrono>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <thread>

#include "rhythm/audio/playback.h"
#include "rhythm/media/soundtrack.h"
#if defined(__ANDROID__)
#include "audio_test_host.h"
#endif

namespace {
using namespace rhythm;
using namespace rhythm::audio;
using namespace std::chrono_literals;
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
PlaybackSnapshot Wait(FilePlayback& playback,
                      const std::function<bool(const PlaybackSnapshot&)>& predicate) {
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (std::chrono::steady_clock::now() < deadline) {
        const auto state = playback.Snapshot();
        if (state.state_ == PlaybackState::kFailed) throw std::runtime_error(state.error_);
        Require(state.volume_ == 0, "source changes preserve muted device master intent");
        if (predicate(state)) return state;
        std::this_thread::sleep_for(5ms);
    }
    throw std::runtime_error("audio.transition_mailbox_timeout");
}
media::SoundtrackSource Source(const std::filesystem::path& path, float gain) {
    media::SoundtrackSource result;
    result.binding_.asset_ = {std::string(64, 'a')};
    result.binding_.gain_ = gain;
    result.binding_.loop_ = true;
    result.file_bytes_ = storage::FileBytes::Open(path, 16 * 1024 * 1024);
    return result;
}
void VerifySerialCut(const std::filesystem::path& directory) {
    auto old = Source(directory / "tone.flac", 0.5F);
    const auto file = old.file_bytes_;
    old.file_bytes_ = {};
    for (std::uint64_t id = 1; id <= 4; ++id) {
        media::AudioClip clip{id, "Concurrent", old.binding_.asset_};
        clip.timing_ = {0, 1, 0, 1};
        clip.gain_ = 0.2F;
        old.binding_.clips_.push_back(clip);
    }
    old.arrangement_ = media::AudioArrangementSource{media::AudioArrangement(old.binding_.clips_),
                                                     {{old.binding_.asset_, {}, file}}};
    auto next = old;
    next.binding_.gain_ = 0.25F;
    FilePlayback playback;
    playback.SetVolume(0);
    playback.LoadSoundtrack(old);
    const auto initial =
            Wait(playback, [](const auto& state) { return state.features_.has_value(); });
    playback.Pause(true);
    Wait(playback, [](const auto& state) { return state.state_ == PlaybackState::kPaused; });
    const auto refused = playback.BeginTransition(next, 0.5);
    const auto failed = Wait(playback, [&](const auto& state) {
        return state.transition_.id_ == refused &&
               state.transition_.state_ == AudioTransitionState::kFailed;
    });
    Require(failed.transition_.error_ == "audio.transition_cursor_budget" && failed.paused_,
            "four plus four fade fails explicitly without replacing accepted audio");
    const auto cancel_id = playback.BeginTransition(next, 0);
    const auto queued = Wait(playback, [&](const auto& state) {
        return state.transition_.id_ == cancel_id &&
               state.transition_.state_ == AudioTransitionState::kQueued;
    });
    Require(queued.paused_ && queued.consumed_frames_ == failed.consumed_frames_ &&
                    playback.CancelTransition(cancel_id),
            "serial cut remains queued while paused and can be canceled");
    playback.Pause(false);
    const auto canceled = Wait(playback, [&](const auto& state) {
        return state.transition_.id_ == cancel_id &&
               state.transition_.state_ == AudioTransitionState::kCanceled;
    });
    const auto accepted = playback.BeginTransition(next, 0);
    const auto complete = Wait(playback, [&](const auto& state) {
        return state.transition_.id_ == accepted &&
               state.transition_.state_ == AudioTransitionState::kCompleted;
    });
    Require(complete.transition_.incoming_presented_ &&
                    complete.transition_.duration_frames_ == 0 &&
                    complete.consumed_frames_ > canceled.consumed_frames_ &&
                    complete.source_generation_ == initial.source_generation_ &&
                    complete.position_seconds_ == complete.transition_.incoming_seconds_,
            "serial four plus four handoff preserves device epoch and waits for consumed new PCM");
    playback.Seek(0.2);
    const auto sought = Wait(playback, [&](const auto& state) {
        return state.features_ && state.generation_ > complete.generation_ &&
               state.position_seconds_ > 0.25;
    });
    Require(sought.features_->rms_ > 0.02F && sought.features_->rms_ < 0.08F,
            "serial handoff retains accepted source gain through later seek");
    playback.Stop();
}
void Run(const std::filesystem::path& directory) {
    auto old = Source(directory / "tone.flac", 0.25F);
    auto next = Source(directory / "tone44100.wav", 0.6F);
    FilePlayback playback;
    playback.SetVolume(0);
    playback.LoadSoundtrack(old);
    const auto ready =
            Wait(playback, [](const auto& state) { return state.features_.has_value(); });
    playback.Pause(true);
    Wait(playback, [](const auto& state) { return state.state_ == PlaybackState::kPaused; });
    const auto canceled_id = playback.BeginTransition(next, 0.5);
    Require(playback.Snapshot().transition_.id_ == canceled_id && playback.Snapshot().paused_,
            "transition intent is visible and preserves pause");
    bool busy = false;
    try {
        (void)playback.BeginTransition(old, 0.5);
    } catch (const std::logic_error&) {
        busy = true;
    }
    Require(busy && !playback.CancelTransition(canceled_id + 1) &&
                    playback.CancelTransition(canceled_id) &&
                    !playback.CancelTransition(canceled_id),
            "one command, stable cancellation identity and no repeated cancel");
    playback.Pause(false);
    const auto canceled = Wait(playback, [&](const auto& state) {
        return state.transition_.id_ == canceled_id &&
               state.transition_.state_ == AudioTransitionState::kCanceled;
    });
    Require(canceled.source_generation_ == ready.source_generation_ &&
                    canceled.transition_.error_.empty(),
            "cancel preserves device/source epoch and reports success independently");

    const auto id = playback.BeginTransition(next, 0.3);
    const auto mixed = Wait(playback, [&](const auto& state) {
        return state.transition_.id_ == id &&
               state.transition_.state_ == AudioTransitionState::kMixing;
    });
    Require(mixed.transition_.incoming_presented_ &&
                    mixed.source_generation_ == ready.source_generation_,
            "mixed incoming clock shares current device epoch");
    const auto completed = Wait(playback, [&](const auto& state) {
        return state.transition_.id_ == id &&
               state.transition_.state_ == AudioTransitionState::kCompleted;
    });
    Require(completed.consumed_frames_ > completed.transition_.end_frame_ &&
                    completed.position_seconds_ == completed.transition_.incoming_seconds_ &&
                    completed.source_generation_ == ready.source_generation_,
            "public completion follows consumption without recreating device");
    playback.Pause(true);
    Wait(playback, [](const auto& state) { return state.state_ == PlaybackState::kPaused; });
    playback.Seek(0.25);
    const auto sought = Wait(playback, [&](const auto& state) {
        return state.state_ == PlaybackState::kPaused &&
               state.source_generation_ > ready.source_generation_;
    });
    Require(sought.position_seconds_ == 0.25 && sought.transition_.id_ == 0,
            "seek uses committed incoming source and clears transition status");
    playback.Pause(false);
    const auto new_signal =
            Wait(playback, [](const auto& state) { return state.features_.has_value(); });
    Require(new_signal.features_->rms_ > ready.features_->rms_ * 1.5F,
            "authored incoming gain survives seek without changing master volume");

    auto broken = next;
    broken.file_bytes_ = {};
    broken.bytes_ = std::make_shared<const std::vector<std::uint8_t>>(32, std::uint8_t{0});
    const auto bad_id = playback.BeginTransition(broken, 0.2);
    const auto rejected = Wait(playback, [&](const auto& state) {
        return state.transition_.id_ == bad_id &&
               state.transition_.state_ == AudioTransitionState::kFailed;
    });
    Require(!rejected.transition_.error_.empty() && rejected.error_.empty() &&
                    rejected.state_ == PlaybackState::kPlaying,
            "preparation rejection keeps accepted playback running");
    Wait(playback, [&](const auto& state) {
        return state.consumed_frames_ > rejected.consumed_frames_ + 4096;
    });

    playback.Pause(true);
    Wait(playback, [](const auto& state) { return state.state_ == PlaybackState::kPaused; });
    const auto stale = playback.BeginTransition(old, 1);
    playback.Stop();
    const auto stop_generation = playback.Snapshot().generation_;
    Wait(playback, [&](const auto& state) { return state.source_generation_ >= stop_generation; });
    std::this_thread::sleep_for(50ms);
    Require(playback.Snapshot().state_ == PlaybackState::kStopped &&
                    playback.Snapshot().transition_.id_ == 0 && !playback.CancelTransition(stale),
            "stop invalidates pending preparation and all late transition publication");
    const auto silence_id = playback.BeginTransition(next, 0.15);
    Wait(playback, [&](const auto& state) {
        return state.transition_.id_ == silence_id &&
               state.transition_.state_ == AudioTransitionState::kQueued;
    });
    Require(playback.CancelTransition(silence_id), "cancel a prepared silent-bus transition");
    const auto silent_canceled = Wait(playback, [&](const auto& state) {
        return state.transition_.id_ == silence_id &&
               state.transition_.state_ == AudioTransitionState::kCanceled;
    });
    Require(silent_canceled.paused_ && silent_canceled.consumed_frames_ == 0,
            "no queued sound means silent-bus cancellation can finish while paused");
    const auto silent_next = playback.BeginTransition(next, 0.15);
    playback.Pause(false);
    const auto silent_completed = Wait(playback, [&](const auto& state) {
        return state.transition_.id_ == silent_next &&
               state.transition_.state_ == AudioTransitionState::kCompleted;
    });
    Require(silent_completed.transition_.incoming_presented_ &&
                    silent_completed.position_seconds_ ==
                            silent_completed.transition_.incoming_seconds_,
            "no prior music still uses one consumed incoming clock");
    playback.Stop();
}
}  // namespace
int main(int argc, char** argv) {
    try {
#if defined(__ANDROID__)
        rhythm::audio::test::InitializeNativeAudioTest();
#endif
        Require(argc == 2, "media fixture directory required");
        Run(std::filesystem::path(argv[1]));
        VerifySerialCut(std::filesystem::path(argv[1]));
        std::cout << "async transitions: stable IDs, pause/cancel, consumed handoff, gain/master, "
                     "seek, failed preparation and stop passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
