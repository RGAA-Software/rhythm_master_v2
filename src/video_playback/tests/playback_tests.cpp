#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include "rhythm/video/playback.h"

namespace {
void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
rhythm::video::PlaybackSnapshot Await(rhythm::video::Playback& playback, double seconds,
                                      std::uint64_t generation, bool loop = false) {
    return playback.Resolve(seconds, generation, loop);
}
void Frame(const rhythm::video::PlaybackSnapshot& snapshot, double pts, int red) {
    Check(snapshot.error_.empty() && snapshot.frame_ &&
                  std::abs(snapshot.frame_->seconds_ - pts) < 0.001 &&
                  snapshot.frame_->rgba_.size() == 64 * 48 * 4 && snapshot.frame_->rgba_[0] == red,
          "wrong timestamp or pixels");
}
}  // namespace
int main(int argc, char** argv) {
    using namespace rhythm;
    try {
        Check(argc == 2, "fixtures directory required");
        const std::filesystem::path directory(argv[1]);
        video::Playback playback(directory / std::filesystem::path(u8"视频.mkv"));
        Frame(Await(playback, 0, 1), 0, 0);
        const auto first = Await(playback, 0.19, 1);
        Frame(first, 0, 0);
        const auto held = Await(playback, 0.19, 1);
        Check(held.frame_ == first.frame_ && held.frame_revision_ == first.frame_revision_,
              "pause/repeated demand reuses immutable frame");
        Frame(Await(playback, 0.2, 1), 0.2, 20);
        Frame(Await(playback, 0.79, 1), 0.6, 60);
        Frame(Await(playback, 0.01, 2), 0, 0);
        const auto end = Await(playback, 3, 2);
        Frame(end, 1.8, 180);
        Check(end.ended_, "EOF holds last frame and reports ended");
        Frame(Await(playback, 2.21, 3, true), 0.2, 20);
        Frame(Await(playback, 4.01, 3, true), 0, 0);
        for (std::uint64_t generation = 4; generation < 50; ++generation)
            playback.Request(generation % 2 ? 1.8 : 0, generation, false);
        const auto latest = Await(playback, 0.4, 50);
        Frame(latest, 0.4, 40);
        Check(latest.frame_->generation_ == 50, "old seek cannot publish into the next generation");
        video::Playback variable(directory / "variable.mkv");
        Frame(Await(variable, 0.1, 1), 0.04, 120);
        Frame(Await(variable, 0.17, 1), 0.16, 120);
        video::Playback reordered(directory / "reordered.mp4");
        const auto reordered_end = Await(reordered, 3, 1);
        Check(reordered_end.error_.empty() && reordered_end.ended_ && reordered_end.frame_ &&
                      std::abs(reordered_end.frame_->seconds_ - 1.8) < 0.001,
              "B frames drain before holding last frame");
        std::ifstream source(directory / std::filesystem::path(u8"视频.mkv"), std::ios::binary);
        auto bytes = std::make_shared<const std::vector<std::uint8_t>>(
                std::istreambuf_iterator<char>(source), std::istreambuf_iterator<char>());
        video::Playback embedded(bytes);
        bytes.reset();
        Frame(Await(embedded, 0.6, 1), 0.6, 60);
        Frame(Await(playback, 1.2, 51), 1.2, 120);
        Frame(embedded.Snapshot(), 0.6, 60);
        video::Playback broken(directory / "broken.bin");
        Check(!Await(broken, 0, 1).error_.empty(), "bad input reports a worker error");
        bool rejected = false;
        try {
            playback.Request(-1, 52, false);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        Check(rejected, "negative request rejected");
        std::stop_source canceled;
        canceled.request_stop();
        rejected = false;
        try {
            playback.Resolve(0, 52, false, canceled.get_token());
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        Check(rejected, "offline canceled demand rejects without replacing current frame");
        Frame(playback.Snapshot(), 1.2, 120);
        Frame(playback.Resolve(0.2, 53, false), 0.2, 20);
        Frame(playback.Resolve(std::nextafter(1.8, 0.0), 54, false, {}, 1.8), 1.6, 160);
        Frame(playback.Resolve(1.8, 54, false), 1.8, 180);
        std::cout << "Video timestamp hold, VFR, seek supersession, loops, EOF and independent "
                     "embedded sources passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
