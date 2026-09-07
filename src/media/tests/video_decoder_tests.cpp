#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <stop_token>
#include <vector>

#include "rhythm/media/video_decoder.h"

namespace {
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
template <typename F>
void Reject(F&& operation) {
    bool rejected = false;
    try {
        operation();
    } catch (const std::exception&) {
        rejected = true;
    }
    Require(rejected, "invalid media operation accepted");
}
void Run(const std::filesystem::path& root) {
    namespace media = rhythm::media;
    media::VideoDecoder video(root / std::filesystem::path(u8"视频.mkv"), 9);
    Require(video.Info().width_ == 64 && video.Info().height_ == 48, "source extent");
    std::vector<media::VideoFrame> frames;
    while (auto frame = video.Read()) {
        const auto index = frames.size();
        Require(frame->generation_ == 9 && frame->rgba_.size() == 64 * 48 * 4,
                "owned bounded frame");
        Require(std::abs(frame->seconds_ - index * 0.2) < 0.001, "presentation timestamp");
        for (std::size_t pixel = 0; pixel < frame->rgba_.size(); pixel += 4) {
            Require(frame->rgba_[pixel] == index * 20 && frame->rgba_[pixel + 1] == 80 &&
                            frame->rgba_[pixel + 2] == 200 && frame->rgba_[pixel + 3] == 255,
                    "lossless RGB conversion");
        }
        frames.push_back(std::move(*frame));
    }
    Require(frames.size() == 10 && !video.Read(), "all frames and stable EOF");
    for (const double time : {0.0, 0.01, 0.2, 0.99, 1.8}) {
        video.Seek(time, 17);
        auto selected = video.Read();
        Require(selected && selected->generation_ == 17 && selected->seconds_ + 1e-9 >= time,
                "seek first frame at or after requested time");
        const auto expected = static_cast<std::size_t>(std::ceil(time * 5 - 1e-9));
        Require(selected->rgba_ == frames[expected].rgba_, "exact seek pixels");
    }
    video.Seek(0, 18);
    Reject([&] { video.Seek(-1, 20); });
    Reject([&] { video.Seek(std::numeric_limits<double>::quiet_NaN(), 20); });
    Reject([&] { video.Seek(100, 20); });
    std::stop_source canceled;
    canceled.request_stop();
    Reject([&] { video.Seek(1, 21, canceled.get_token()); });
    Require(video.Read()->generation_ == 18, "failed seek preserves previous decoder");
    Reject([&] { video.Read(canceled.get_token()); });
    Require(std::abs(video.Read()->seconds_ - 0.2) < 0.001, "pre-canceled read consumes no frame");

    media::VideoDecoder reordered(root / "reordered.mp4");
    int count = 0;
    double previous = -1;
    while (auto frame = reordered.Read()) {
        Require(frame->seconds_ > previous, "B frames in presentation order");
        previous = frame->seconds_;
        ++count;
    }
    Require(count == 10, "B frame EOF drain");
    media::VideoDecoder vfr(root / "variable.mkv");
    std::vector<double> timestamps;
    while (auto frame = vfr.Read()) timestamps.push_back(frame->seconds_);
    Require(timestamps.size() == 4 && std::abs(timestamps[1] - 0.04) < 0.001 &&
                    std::abs(timestamps[2] - 0.16) < 0.001 &&
                    std::abs(timestamps[3] - 0.24) < 0.001,
            "variable frame rate timestamps");
    media::VideoDecoder rotated(root / "rotated.mp4");
    Require(std::abs(std::abs(rotated.Info().clockwise_rotation_) - 90) < 0.01,
            "display rotation metadata");
    Reject([&] { media::VideoDecoder invalid(root / "broken.bin"); });
    Reject([&] { media::VideoDecoder invalid(root); });
    Reject([&] { media::VideoDecoder invalid("https://example.invalid/movie.mp4"); });
    std::cout << "Video timelines, lossless pixels, B-frame drain and local I/O passed\n";
    media::VideoDecoder image(root / "alpha.png");
    auto still = image.Read();
    Require(still && !image.Read(), "still image single frame");
    Require(still->rgba_[0] == 120 && still->rgba_[1] == 40 && still->rgba_[2] == 200 &&
                    still->rgba_[3] == 64,
            "PNG retains straight alpha");
    std::ifstream input(root / "alpha.png", std::ios::binary);
    std::vector<std::uint8_t> embedded((std::istreambuf_iterator<char>(input)), {});
    media::VideoDecoder memory(std::span<const std::uint8_t>(embedded), 31);
    embedded.clear();
    auto decoded = memory.Read();
    Require(decoded && decoded->rgba_ == still->rgba_ && decoded->generation_ == 31,
            "embedded media owns bytes independently of caller storage");
    memory.Seek(0, 32);
    Require(memory.Read()->generation_ == 32 && !memory.Read(), "embedded seek and EOF");
    Reject([&] { media::VideoDecoder invalid(std::span<const std::uint8_t>{}); });
}
}  // namespace
int main(int argc, char* argv[]) {
    try {
        if (argc != 2) throw std::invalid_argument("usage: media_video_tests <fixture directory>");
        Run(argv[1]);
        std::cout << "Video: exact RGBA/alpha, PTS/VFR/B-frame drain, rotation, seek, cancellation "
                     "and local I/O passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
