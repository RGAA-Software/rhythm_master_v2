#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

#include "audio_test_host.h"
#include "music_playback.h"

namespace {
using namespace rhythm;
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void Run(const std::filesystem::path& directory) {
    android_host::MusicPlayback music(directory);
    music.SetVolume(0);
    media::SoundtrackSource source;
    source.binding_.asset_ = {std::string(64, 'a')};
    source.binding_.gain_ = 0.5F;
    source.file_bytes_ = storage::FileBytes::Open(directory / "tone.flac", 1024 * 1024);
    music.Open(source);
    const auto wait = [&](const auto& predicate) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (std::chrono::steady_clock::now() < deadline) {
            const auto frame = music.Frame();
            if (frame.failed_) throw std::runtime_error(frame.audio_.error_);
            Require(frame.audio_.volume_ == 0, "authored gain never overwrites device master");
            if (predicate(frame)) return frame;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        throw std::runtime_error("android.music_host_timeout");
    };
    wait([](const auto& frame) { return frame.audio_.state_ == audio::PlaybackState::kEnded; });
    music.Apply({false, 0.2});
    const auto replay = wait([](const auto& frame) {
        return frame.inputs_.audio_ && frame.playback_->seconds_ > 0.3;
    });
    Require(replay.inputs_.audio_->rms_ > 0.1F && replay.inputs_.audio_->rms_ < 0.15F,
            "replay preserves authored gain");
    music.SetLoop(true);
    source.binding_.gain_ = 0.25F;
    source.binding_.loop_ = true;
    const auto id = music.BeginSoundtrackTransition(source, 0.1, false);
    const auto completed = wait([&](const auto& frame) {
        return frame.audio_.transition_.id_ == id &&
               frame.audio_.transition_.state_ == audio::AudioTransitionState::kCompleted;
    });
    music.AdoptSoundtrack(source, id);
    const auto adopted = music.Frame();
    Require(adopted.audio_.source_generation_ == completed.audio_.source_generation_ &&
                    adopted.audio_.consumed_frames_ >= completed.audio_.consumed_frames_ &&
                    adopted.audio_.transition_.id_ == id && music.Loop(),
            "metadata adoption preserves continuous device epoch and source time");
    music.Apply({false, 0.2});
    const auto quiet = wait([](const auto& frame) {
        return frame.inputs_.audio_ && frame.playback_->seconds_ > 0.3;
    });
    Require(quiet.inputs_.audio_->rms_ > 0.05F && quiet.inputs_.audio_->rms_ < 0.08F,
            "seek after adoption retains incoming authored gain");
    music.SetSuspended(true);
    const auto paused_id = music.BeginSoundtrackTransition(source, 0.5, false);
    wait([](const auto& frame) { return frame.audio_.state_ == audio::PlaybackState::kPaused; });
    Require(music.CancelSoundtrackTransition(paused_id), "cancel suspended transition");
    music.SetSuspended(false);
    wait([](const auto& frame) {
        return frame.audio_.transition_.state_ == audio::AudioTransitionState::kCanceled;
    });
    music.Clear();
    Require(!music.Frame().playback_, "clear deselects audio clock");
}
}  // namespace
int main(int argc, char** argv) {
    try {
        rhythm::audio::test::InitializeNativeAudioTest();
        if (argc != 2) throw std::invalid_argument("expected media directory");
        Run(argv[1]);
        std::cout << "Android music host: gain, continuous adoption, replay and suspended cancel "
                     "passed (dummy device)\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
