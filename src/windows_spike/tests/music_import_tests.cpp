#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <thread>

#include "music_playback.h"

namespace {
void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
}  // namespace
int main(int argc, char* argv[]) {
    using namespace rhythm;
    using namespace std::chrono_literals;
    try {
        Check(argc == 3, "music_import <fixture> <fresh test directory>");
        const auto directory =
                std::filesystem::path(argv[2]) /
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        Check(std::filesystem::create_directories(directory / "cache"), "fresh test directory");
        const auto cache = directory / "cache";
        const auto first = cache / "music-first.media", second = cache / "music-second.media";
        const auto outside = directory / "music-original.media";
        for (const auto& path : {first, second, outside}) std::filesystem::copy_file(argv[1], path);
        {
            android_host::MusicPlayback music(cache);
            music.SetVolume(0);
            Check(!music.Open(outside) && std::filesystem::exists(outside),
                  "source outside private cache is never adopted or removed");
            Check(music.Open(first), "open imported music");
            const auto wait =
                    [&](const std::function<bool(const android_host::MusicFrame&)>& ready) {
                        const auto deadline = std::chrono::steady_clock::now() + 5s;
                        while (std::chrono::steady_clock::now() < deadline) {
                            const auto frame = music.Frame();
                            Check(!frame.failed_, "music decode failed");
                            if (ready(frame)) return frame;
                            std::this_thread::sleep_for(5ms);
                        }
                        throw std::runtime_error("music import wait timed out");
                    };
            wait([](const auto& frame) { return frame.inputs_.audio_.has_value(); });
            music.SetSuspended(true);
            std::this_thread::sleep_for(60ms);
            const auto held = music.Frame().playback_->seconds_;
            std::this_thread::sleep_for(80ms);
            Check(music.Frame().playback_->seconds_ == held, "background freezes playback");
            music.Apply({true, {}});  // Audio focus loss while suspended.
            music.SetSuspended(false);
            std::this_thread::sleep_for(60ms);
            Check(music.Frame().playback_->paused_ && music.Frame().playback_->seconds_ == held,
                  "focus loss cancels automatic foreground resume");
            music.Apply({false, 2});
            wait([](const auto& frame) { return frame.playback_->seconds_ > 2; });
            Check(music.Open(second) && std::filesystem::exists(first),
                  "replacement keeps the old worker source alive");
            wait([](const auto& frame) { return frame.inputs_.audio_.has_value(); });
            Check(!std::filesystem::exists(first) && std::filesystem::exists(second),
                  "worker acknowledgment releases retired file only");
            Check(!music.Open(second) && std::filesystem::exists(second), "duplicate active path");
            std::ifstream input(outside, std::ios::binary);
            const media::SoundtrackSource embedded{
                    {{}, "Embedded", 0, true},
                    std::make_shared<const std::vector<std::uint8_t>>(
                            std::istreambuf_iterator<char>(input),
                            std::istreambuf_iterator<char>())};
            music.Open(embedded);
            wait([](const auto& frame) { return frame.inputs_.audio_.has_value(); });
            Check(!std::filesystem::exists(second),
                  "embedded source retires previous file after acknowledgment");
            music.Apply({true, 0.5});
            wait([](const auto& frame) {
                return frame.playback_->paused_ && frame.playback_->seconds_ == 0.5;
            });
            music.Clear();
            Check(!music.Frame().playback_, "cleared package has no stale music clock");
            music.Open(embedded);
            wait([](const auto& frame) { return frame.inputs_.audio_.has_value(); });
        }
        Check(!std::filesystem::exists(second) && std::filesystem::exists(outside),
              "destruction joins decoder before removing owned cache files");
        std::cout << "music imports: private leases, replacement, suspension/focus and cleanup "
                     "passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
