#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

#include "rhythm/audio_ui/audio_panel.h"
#include "rhythm/media/audio_mixer.h"

int main(int argc, char** argv) {
    using namespace rhythm;
    try {
        const auto check = [](bool value) {
            if (!value) throw std::runtime_error("arrangement.host_contract");
        };
        check(argc == 2);
        audio_ui::AudioPanel panel;
        panel.SetVolume(0);
        const assets::AssetId id{std::string(64, 'a')};
        media::AudioClip clip{1, "Test", id};
        clip.timing_ = {0, 0.75, 0, 0.75};
        const media::AudioArrangement arrangement({clip});
        panel.LoadArrangement(media::AudioArrangementFiles{arrangement, {{id, argv[1]}}});
        const auto wait = [&](const auto& predicate) {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            for (;;) {
                const auto frame = panel.Frame();
                if (predicate(frame)) return frame;
                check(std::chrono::steady_clock::now() < deadline);
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        };
        wait([](const auto& frame) { return frame.features_ && frame.features_->rms_ > 0.01F; });
        runtime::PlaybackCommand command;
        command.paused_ = true;
        command.seek_ = 0.375;
        panel.ApplyPlayback(command);
        wait([](const auto& frame) {
            return frame.playback_ && frame.playback_->paused_ &&
                   frame.playback_->seconds_ == 0.375;
        });
        check(!panel.SelectedFile());
        command = {};
        command.paused_ = false;
        panel.ApplyPlayback(command);
        wait([](const auto& frame) {
            return frame.playback_ && frame.playback_->seconds_ == 0.75;
        });
        command.seek_ = 0.2;
        panel.ApplyPlayback(command);
        wait([](const auto& frame) {
            return frame.features_ && frame.playback_ && !frame.playback_->paused_ &&
                   frame.playback_->seconds_ >= 0.2 && frame.playback_->seconds_ < 0.75;
        });
        media::SoundtrackSource packaged;
        packaged.binding_ = {id, "Packaged", 0.5F, false, {clip}};
        packaged.arrangement_ = media::AudioArrangementSource{
                arrangement, {{id, {}, storage::FileBytes::Open(argv[1], 16 * 1024 * 1024)}}};
        panel.LoadSoundtrack(packaged);
        wait([](const auto& frame) { return frame.features_ && frame.features_->rms_ > 0.01F; });
        check(panel.Volume() == 0 && panel.Frame().file_.volume_ == 0);
        wait([](const auto& frame) { return frame.file_.state_ == audio::PlaybackState::kEnded; });
        panel.ApplyPlayback({false, 0.2});
        const auto replay = wait([](const auto& frame) {
            return frame.features_ && frame.playback_->seconds_ > 0.3;
        });
        check(replay.features_->rms_ > 0.1F && replay.features_->rms_ < 0.15F);
        panel.SetLoop(true);
        packaged.binding_.gain_ = 0.25F;
        packaged.binding_.loop_ = true;
        const auto transition = panel.BeginSoundtrackTransition(packaged, 0.1, false);
        const auto completed = wait([&](const auto& frame) {
            return frame.file_.transition_.id_ == transition &&
                   frame.file_.transition_.state_ == audio::AudioTransitionState::kCompleted;
        });
        panel.AdoptSoundtrack(packaged, transition);
        const auto adopted = panel.Frame();
        check(adopted.file_.source_generation_ == completed.file_.source_generation_ &&
              adopted.file_.consumed_frames_ >= completed.file_.consumed_frames_ &&
              adopted.file_.transition_.id_ == transition && panel.Loop() && panel.Volume() == 0);
        panel.ApplyPlayback({false, 0.2});
        const auto quiet = wait([](const auto& frame) {
            return frame.features_ && frame.playback_->seconds_ > 0.3;
        });
        check(quiet.features_->rms_ > 0.05F && quiet.features_->rms_ < 0.08F);
        check(quiet.playback_->continuous_.has_value());
        const auto epoch = quiet.playback_->continuous_->generation_;
        const auto looped = wait([&](const auto& frame) {
            return frame.playback_ &&
                   frame.playback_->generation_ != quiet.playback_->generation_ &&
                   frame.file_.state_ == audio::PlaybackState::kPlaying;
        });
        check(looped.playback_->continuous_->generation_ == epoch &&
              looped.playback_->continuous_->seconds_ > quiet.playback_->continuous_->seconds_);
        panel.ApplyPlayback({false, 0.2});
        const auto seeked = wait([&](const auto& frame) {
            return frame.playback_ && frame.playback_->continuous_->generation_ != epoch &&
                   frame.file_.state_ == audio::PlaybackState::kPlaying;
        });
        check(seeked.playback_->continuous_->seconds_ < looped.playback_->continuous_->seconds_);
        bool stale_rejected = false;
        try {
            panel.AdoptSoundtrack(packaged, transition);
        } catch (const std::logic_error&) {
            stale_rejected = true;
        }
        check(stale_rejected);
        panel.ClearFile();
        check(!panel.Frame().playback_);
        std::cout << "Audio host: arrangement, package gain, master volume, consumed transition "
                     "adoption, pause/seek/restart and clear "
                     "passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
