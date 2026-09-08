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
        packaged.binding_ = {id, "Packaged", 0, false, {clip}};
        packaged.arrangement_ = media::AudioArrangementSource{
                arrangement, {{id, {}, storage::FileBytes::Open(argv[1], 16 * 1024 * 1024)}}};
        panel.LoadSoundtrack(packaged);
        wait([](const auto& frame) { return frame.features_ && frame.features_->rms_ > 0.01F; });
        panel.ClearFile();
        check(!panel.Frame().playback_);
        std::cout << "Audio host: file arrangement, package mix, pause/seek/restart and clear "
                     "passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
