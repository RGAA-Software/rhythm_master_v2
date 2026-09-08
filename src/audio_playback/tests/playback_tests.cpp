#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <thread>

#include "rhythm/audio/playback.h"
#include "rhythm/media/audio_arrangement.h"
#include "rhythm/storage/file_bytes.h"

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
                    repeated.queued_frames_ <= 24000 &&
                    repeated.source_generation_ == before_loop &&
                    repeated.state_ == PlaybackState::kPlaying,
            "repeat clears old analysis and retains queue bounds");
    Require(repeated.consumed_frames_ > 2 * 48000 &&
                    repeated.submitted_frames_ >= repeated.consumed_frames_ &&
                    repeated.submitted_frames_ - repeated.consumed_frames_ <= 32768,
            "device counters and analysis consumption continue across two complete loops");
    playback.SetLoop(false);
    Wait(playback, [](const auto& value) { return value.state_ == PlaybackState::kEnded; });
    std::ifstream input(directory / "tone.flac", std::ios::binary);
    auto bytes = std::make_shared<const std::vector<std::uint8_t>>(
            std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    const std::weak_ptr<const std::vector<std::uint8_t>> lifetime = bytes;
    playback.Load(bytes);
    bytes.reset();
    Wait(playback, [](const auto& value) { return value.features_.has_value(); });
    playback.Pause(true);
    Wait(playback, [](const auto& value) { return value.state_ == PlaybackState::kPaused; });
    playback.Seek(0.25);
    const auto embedded = Wait(playback, [](const auto& value) {
        return value.state_ == PlaybackState::kPaused && value.position_seconds_ == 0.25;
    });
    Require(!lifetime.expired() && !embedded.features_, "embedded seek retains shared source");
    playback.SetLoop(true);
    playback.Pause(false);
    Wait(playback, [&](const auto& value) {
        return value.generation_ > embedded.generation_ && value.features_.has_value();
    });
    playback.Stop();
    const auto release_deadline = std::chrono::steady_clock::now() + 2s;
    while (!lifetime.expired() && std::chrono::steady_clock::now() < release_deadline)
        std::this_thread::sleep_for(5ms);
    Require(lifetime.expired(), "stopped worker releases embedded source");
    auto range = rhythm::storage::FileBytes::Open(directory / "tone.flac", 16 * 1024 * 1024);
    playback.Load(range);
    range = {};
    const auto file_signal = Wait(playback, [](const auto& value) {
        return value.features_ && value.features_->rms_ > 0.01f;
    });
    Require(file_signal.queued_frames_ <= 24000,
            "file-range playback keeps the same bounded queue");
    playback.Pause(true);
    Wait(playback, [](const auto& value) { return value.state_ == PlaybackState::kPaused; });
    playback.Seek(0.125);
    const auto file_seek = Wait(playback, [](const auto& value) {
        return value.state_ == PlaybackState::kPaused && value.position_seconds_ == 0.125;
    });
    playback.SetLoop(true);
    playback.Pause(false);
    Wait(playback, [&](const auto& value) {
        return value.generation_ > file_seek.generation_ && value.features_.has_value();
    });
    playback.Stop();
    const auto stopped_generation = playback.Snapshot().generation_;
    Wait(playback, [&](const auto& value) {
        return value.source_generation_ >= stopped_generation &&
               value.state_ == PlaybackState::kStopped;
    });
    const rhythm::assets::AssetId asset{std::string(64, 'a')};
    rhythm::media::AudioClip clip{1, "First", asset};
    clip.timing_ = {0, 0.5, 0.1, 0.6};
    clip.gain_ = 0.4F;
    auto second = clip;
    second.id_ = 2;
    second.title_ = "Second";
    second.timing_.start_ = 0.25;
    playback.SetLoop(false);
    playback.Load(rhythm::media::AudioArrangementSource{
            rhythm::media::AudioArrangement({clip, second}),
            {{asset,
              {},
              rhythm::storage::FileBytes::Open(directory / "tone.flac", 16 * 1024 * 1024)}}});
    const auto mixed = Wait(playback, [](const auto& value) {
        return value.features_ && value.features_->rms_ > 0.01F;
    });
    Require(mixed.duration_seconds_ == 0.75 && mixed.queued_frames_ <= 24000,
            "arrangement uses existing output and mixed-PCM analysis");
    playback.Pause(true);
    Wait(playback, [](const auto& value) { return value.state_ == PlaybackState::kPaused; });
    playback.Seek(0.375);
    const auto mixed_seek = Wait(playback, [](const auto& value) {
        return value.state_ == PlaybackState::kPaused && value.position_seconds_ == 0.375;
    });
    Require(!mixed_seek.features_, "arrangement seek clears prior analysis");
    playback.SetLoop(true);
    playback.Pause(false);
    const auto mixed_loop = Wait(playback, [&](const auto& value) {
        return value.generation_ > mixed_seek.generation_ && value.features_.has_value();
    });
    Require(mixed_loop.source_generation_ == mixed_seek.source_generation_ &&
                    mixed_loop.submitted_frames_ >= mixed_loop.consumed_frames_,
            "arrangement loops retain the same device stream");
    playback.SetLoop(false);
    const auto mixed_end =
            Wait(playback, [](const auto& value) { return value.state_ == PlaybackState::kEnded; });
    Require(mixed_end.position_seconds_ == 0.75, "arrangement ends at exact final sample");
    playback.Stop();
    std::cout << "file and multitrack playback: device output, canonical analysis, bounded queue, "
                 "pause, seek, "
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
