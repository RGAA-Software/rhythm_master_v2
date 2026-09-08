#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

#include "rhythm/content/music_authoring.h"

int main(int argc, char** argv) {
    using namespace rhythm;
    try {
        const auto check = [](bool value) {
            if (!value) throw std::runtime_error("music.import_contract");
        };
        check(argc == 3);
        const std::filesystem::path fixtures(argv[1]);
        const auto assets = std::filesystem::path(argv[2]) / "assets";
        content::MusicAuthoring importer;
        editor::Snapshot snapshot;
        snapshot.document_.id_ = "music.authoring";
        const auto append = [&](const std::filesystem::path& path, bool enabled) {
            check(importer.Start(snapshot, assets, path, 0.5F, true, enabled));
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            for (;;) {
                if (auto result = importer.Take()) {
                    if (!result->snapshot_) throw std::runtime_error(result->error_);
                    return *result->snapshot_;
                }
                check(std::chrono::steady_clock::now() < deadline);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        };
        snapshot = append(fixtures / "tone.flac", true);
        check(snapshot.soundtrack_ && snapshot.soundtrack_->clips_.size() == 1 &&
              snapshot.assets_.size() == 1);
        snapshot = append(fixtures / std::filesystem::path(u8"音乐 ramp.wav"), true);
        check(snapshot.soundtrack_->clips_.size() == 2 && snapshot.assets_.size() == 2 &&
              snapshot.soundtrack_->clips_[1].timing_.start_ == 1);
        check(content::UnbindSoundtrack(snapshot).assets_.empty());
        snapshot = append(fixtures / "tone.flac", false);
        check(snapshot.soundtrack_->clips_.empty() && snapshot.assets_.size() == 1);
        snapshot = append(fixtures / std::filesystem::path(u8"音乐 ramp.wav"), true);
        check(snapshot.soundtrack_->clips_.size() == 2 &&
              snapshot.soundtrack_->clips_[0].timing_.duration_ == 1);
        std::cout << "Music authoring: append distinct sources, native durations, legacy "
                     "conversion and unbind passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
