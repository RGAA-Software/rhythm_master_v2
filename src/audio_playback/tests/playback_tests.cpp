#include <chrono>
#include <filesystem>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <thread>

#include "rhythm/audio/playback.h"

namespace {
using namespace std::chrono_literals;
void Require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}
rhythm::audio::PlaybackSnapshot Wait(
        rhythm::audio::FilePlayback& playback,
        const std::function<bool(const rhythm::audio::PlaybackSnapshot&)>& predicate) {
    const auto end = std::chrono::steady_clock::now() + 5s;
    while (std::chrono::steady_clock::now() < end) {
        const auto state = playback.Snapshot();
        if (predicate(state)) {
            return state;
        }
        if (state.state_ == rhythm::audio::PlaybackState::kFailed) {
            throw std::runtime_error("playback failed");
        }
        std::this_thread::sleep_for(5ms);
    }
    throw std::runtime_error("playback wait timed out");
}
void Run(const std::filesystem::path& directory) {
    using rhythm::audio::PlaybackState;
    rhythm::audio::FilePlayback playback;
    playback.SetVolume(0);
    playback.Load(directory / "tone.flac");
    const auto playing =
            Wait(playback, [](const auto& value) { return value.features_.has_value(); });
    Require(playing.state_ == PlaybackState::kPlaying && playing.queued_frames_ <= 24000,
            "playback features and queue budget");
    Require(playing.features_->generation_ == playing.generation_, "playback feature generation");
    playback.Pause(true);
    Require(playback.Snapshot().paused_, "pause intent is visible before worker acknowledgment");
    const auto paused = Wait(
            playback, [](const auto& value) { return value.state_ == PlaybackState::kPaused; });
    std::this_thread::sleep_for(100ms);
    Require(playback.Snapshot().position_seconds_ == paused.position_seconds_,
            "pause freezes media clock");
    playback.Seek(0.4);
    Require(playback.Snapshot().position_seconds_ == 0.4 && playback.Snapshot().paused_,
            "loading seek publishes requested time without briefly jumping to zero");
    const auto sought = Wait(playback, [&](const auto& value) {
        return value.generation_ > paused.generation_ && value.state_ == PlaybackState::kPaused;
    });
    Require(!sought.features_, "seek clears previous analysis");
    playback.Pause(false);
    const auto ended =
            Wait(playback, [](const auto& value) { return value.state_ == PlaybackState::kEnded; });
    Require(ended.position_seconds_ == 1.0 && ended.queued_frames_ == 0,
            "file playback drains at exact media end");
    Require(ended.features_ && ended.features_->generation_ == ended.generation_,
            "seek rebuilds canonical analysis");
    for (int index = 0; index < 12; ++index) {
        playback.Load(directory / "tone.flac");
        playback.Seek(0.1 * (index % 5));
    }
    playback.Stop();
    const auto stopped = playback.Snapshot();
    std::this_thread::sleep_for(150ms);
    const auto stable = playback.Snapshot();
    Require(stable.state_ == PlaybackState::kStopped && stable.generation_ == stopped.generation_ &&
                    !stable.features_,
            "superseded workers cannot publish after stop");
    playback.Load(directory / "missing.flac");
    Wait(playback, [](const auto& value) { return value.state_ == PlaybackState::kFailed; });
    playback.Load(directory / "tone.flac");
    Wait(playback, [](const auto& value) { return value.features_.has_value(); });
    playback.SetLoop(true);
    const auto before_loop = playback.Snapshot().generation_;
    const auto repeated = Wait(playback, [&](const auto& value) {
        return value.generation_ >= before_loop + 2 && value.features_.has_value();
    });
    Require(repeated.features_->generation_ == repeated.generation_ &&
                    repeated.queued_frames_ <= 24000,
            "repeat clears old analysis and retains queue bounds");
    playback.SetLoop(false);
    Wait(playback, [](const auto& value) { return value.state_ == PlaybackState::kEnded; });
    std::cout << "file playback: device output, canonical analysis, bounded queue, pause, seek, "
                 "EOF, bounded repeat, latest-request cancellation and recovery passed\n";
}
}  // namespace
int main(int argc, char* argv[]) {
    try {
        Require(argc == 2, "usage: audio_playback_tests <media fixture directory>");
        const std::string argument(argv[1]);
        Run(std::filesystem::path(std::u8string(argument.begin(), argument.end())));
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
