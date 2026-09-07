#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <numbers>
#include <stdexcept>

#include "rhythm/media/audio_decoder.h"
#include "rhythm/media/av_writer.h"
#include "rhythm/media/video_decoder.h"

namespace {
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
template <typename Function>
void Reject(Function function) {
    bool rejected = false;
    try {
        function();
    } catch (const std::exception&) {
        rejected = true;
    }
    Require(rejected, "invalid encoding operation accepted");
}
float Audio(std::uint64_t sample, int channel) {
    return static_cast<float>((channel ? 0.1 : 0.2) *
                              std::sin(2 * std::numbers::pi * (channel ? 880 : 440) *
                                       static_cast<double>(sample) / 48000));
}
std::vector<std::uint8_t> Picture(int frame) {
    std::vector<std::uint8_t> pixels(160 * 96 * 4);
    for (int y = 0; y < 96; ++y)
        for (int x = 0; x < 160; ++x) {
            const auto index = static_cast<std::size_t>((y * 160 + x) * 4);
            pixels[index] = y < 48 ? static_cast<std::uint8_t>(80 + (frame % 40) * 3) : 20;
            pixels[index + 1] = y < 48 ? 40 : 100;
            pixels[index + 2] = y < 48 ? 30 : 200;
            pixels[index + 3] = 255;
        }
    return pixels;
}
void RoundTrip(const std::filesystem::path& path, rhythm::media::VideoCodec codec, int fps = 30,
               int total_frames = 75) {
    using namespace rhythm::media;
    EncodingSettings settings{160, 96, static_cast<std::uint32_t>(fps), 800000, codec, true};
    const auto samples_per_frame = static_cast<std::size_t>(48000 / fps);
    const auto total_samples = total_frames * samples_per_frame;
    {
        AvWriter writer(path, settings);
        for (int frame = 0; frame < total_frames; ++frame) {
            writer.WriteVideo(Picture(frame));
            std::vector<float> audio(samples_per_frame * 2);
            for (std::size_t sample = 0; sample < samples_per_frame; ++sample)
                for (int channel = 0; channel < 2; ++channel)
                    audio[sample * 2 + channel] =
                            Audio(frame * samples_per_frame + sample, channel);
            writer.WriteAudio(audio);
        }
        writer.Finish();
        Reject([&] { writer.WriteVideo(Picture(0)); });
    }
    VideoDecoder video(path);
    Require(video.Info().width_ == 160 && video.Info().height_ == 96, "encoded dimensions");
    int frames = 0;
    while (const auto frame = video.Read()) {
        Require(std::abs(frame->seconds_ - static_cast<double>(frames) / fps) < 0.00001,
                "encoded frame timestamp");
        const auto expected = Picture(frames);
        for (const int y : {24, 72})
            for (int channel = 0; channel < 3; ++channel) {
                const auto offset = static_cast<std::size_t>((y * 160 + 80) * 4 + channel);
                Require(std::abs(static_cast<int>(frame->rgba_[offset]) - expected[offset]) <= 12,
                        "encoded SDR colors or orientation");
            }
        ++frames;
    }
    Require(frames == total_frames, "encoder drain retains every submitted video frame");
    AudioDecoder audio(path);
    std::uint64_t samples = 0;
    double error = 0;
    while (const auto block = audio.Read()) {
        Require(block->first_sample_ == samples, "encoded audio continuity");
        for (std::size_t sample = 0; sample < block->samples_.size() / 2; ++sample)
            for (int channel = 0; channel < 2; ++channel) {
                const auto difference =
                        block->samples_[sample * 2 + channel] - Audio(samples + sample, channel);
                error += difference * difference;
            }
        samples += block->samples_.size() / 2;
    }
    std::cout << path.filename().string() << " video_frames=" << frames
              << " audio_samples=" << samples << " audio_mse=" << error / (samples * 2) << '\n';
    Require(samples == total_samples,
            "AAC padding must not change the requested soundtrack duration");
    Require(error / (samples * 2) < 0.002, "AAC stereo samples remain aligned with video time");
    audio.Seek(total_samples - 1, 17);
    const auto last = audio.Read();
    Require(last && last->first_sample_ == total_samples - 1 && last->generation_ == 17 &&
                    last->samples_.size() == 2 && !audio.Read(),
            "AAC seek preserves exact final sample and excludes padding");
    audio.Seek(total_samples, 18);
    Require(!audio.Read(), "AAC seek to exact presented EOF");
    Reject([&] { audio.Seek(total_samples + 1, 19); });
}
}  // namespace
int main(int argc, char* argv[]) {
    using namespace rhythm::media;
    try {
        Require(argc == 2, "av_writer_tests output");
        const auto root =
                std::filesystem::path(argv[1]) /
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        std::filesystem::create_directories(root);
        RoundTrip(root / "mpeg4-aac.mp4", VideoCodec::kMpeg4);
        RoundTrip(root / "single-frame.mp4", VideoCodec::kMpeg4, 60, 1);
        RoundTrip(root / "24fps.mp4", VideoCodec::kMpeg4, 24, 29);
        RoundTrip(root / "25fps.mp4", VideoCodec::kMpeg4, 25, 31);
        RoundTrip(root / "aligned-aac.mp4", VideoCodec::kMpeg4, 30, 64);
#ifdef _WIN32
        RoundTrip(root / "h264-aac.mp4", VideoCodec::kH264);
#endif
        EncodingSettings settings{160, 96, 30, 800000, VideoCodec::kMpeg4, false};
        {
            AvWriter silent(root / "silent.mp4", settings);
            for (int frame = 0; frame < 3; ++frame) silent.WriteVideo(Picture(frame));
            silent.Finish();
        }
        Reject([&] { AudioDecoder absent(root / "silent.mp4"); });
        Reject([&] { AvWriter exists(root / "silent.mp4", settings); });
        std::stop_source stop;
        {
            AvWriter canceled(root / "partial.mp4", settings, stop.get_token());
            canceled.WriteVideo(Picture(0));
            stop.request_stop();
            Reject([&] { canceled.WriteVideo(Picture(1)); });
            Reject([&] { canceled.Finish(); });
        }
        Reject([&] { AvWriter canceled(root / "never-created.mp4", settings, stop.get_token()); });
        Require(!std::filesystem::exists(root / "never-created.mp4"),
                "pre-cancel creates no output");
        settings.width_ = 161;
        Reject([&] { AvWriter invalid(root / "odd.mp4", settings); });
        Require(!std::filesystem::exists(root / "odd.mp4"), "invalid profile creates no output");
        settings.width_ = 160;
        settings.audio_ = true;
        {
            AvWriter overlead(root / "overlead.mp4", settings);
            for (int frame = 0; frame < 30; ++frame) overlead.WriteVideo(Picture(frame));
            Reject([&] { overlead.WriteVideo(Picture(30)); });
            Reject([&] { overlead.WriteAudio(std::vector<float>(3200)); });
        }
        {
            AvWriter oversized(root / "oversized.mp4", settings);
            Reject([&] { oversized.WriteAudio(std::vector<float>(8194)); });
            Reject([&] { oversized.Finish(); });
        }
        {
            AvWriter mismatch(root / "mismatch.mp4", settings);
            mismatch.WriteVideo(Picture(0));
            Reject([&] { mismatch.Finish(); });
        }
        std::cout << "encoding round-trips, bounded input, cancellation and explicit finalization "
                     "passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
