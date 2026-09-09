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
}
}  // namespace
int main(int argc, char** argv) {
    try {
#if defined(__ANDROID__)
        rhythm::audio::test::InitializeNativeAudioTest();
#endif
        Require(argc == 2, "media fixture directory required");
        Run(std::filesystem::path(argv[1]));
        std::cout << "async transitions: stable IDs, pause/cancel, consumed handoff, gain/master, "
                     "seek, failed preparation and stop passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
