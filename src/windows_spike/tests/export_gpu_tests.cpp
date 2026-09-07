#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>

#include "rhythm/export/render_export.h"
#include "rhythm/media/audio_decoder.h"
#include "rhythm/media/video_decoder.h"
#include "rhythm/platform/host.h"

namespace {
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
std::vector<std::uint64_t> ReviewVideo(const std::filesystem::path& path, int expected_frames) {
    rhythm::media::VideoDecoder decoder(path);
    std::vector<std::uint64_t> hashes;
    while (const auto frame = decoder.Read()) {
        Require(std::abs(frame->seconds_ - hashes.size() / 30.0) < 0.00001,
                "offline frame PTS changed");
        Require(frame->rgba_.size() == 640 * 360 * 4, "offline dimensions changed");
        std::uint64_t hash = 14695981039346656037ULL;
        for (const auto channel : frame->rgba_) hash = (hash ^ channel) * 1099511628211ULL;
        hashes.push_back(hash);
        if (hashes.size() == 120) {
            double light = 0;
            for (std::size_t pixel = 0; pixel < frame->rgba_.size(); pixel += 4)
                light += frame->rgba_[pixel] + frame->rgba_[pixel + 1] + frame->rgba_[pixel + 2];
            std::cout << path.filename().string() << " final_mean_rgb=" << light / (640 * 360 * 3)
                      << '\n';
            Require(light / (640 * 360 * 3) > 1, "offline performance output is blank");
        }
    }
    Require(hashes.size() == expected_frames, "offline export lost a video frame");
    Require(std::set(hashes.begin(), hashes.end()).size() > 50,
            "offline animation did not advance");
    return hashes;
}
std::vector<float> ReviewAudio(const std::filesystem::path& path, std::size_t expected_samples,
                               bool source = false) {
    rhythm::media::AudioDecoder decoder(path);
    std::vector<float> samples;
    while (const auto block = decoder.Read()) {
        Require(block->first_sample_ == samples.size() / 2, "offline audio discontinuity");
        samples.insert(samples.end(), block->samples_.begin(), block->samples_.end());
        if (source && samples.size() >= expected_samples * 2) break;
        Require(samples.size() <= expected_samples * 2,
                "offline soundtrack exceeded the video duration");
    }
    Require(samples.size() >= expected_samples * 2, "offline soundtrack is truncated");
    samples.resize(expected_samples * 2);
    return samples;
}
}  // namespace
int main(int argc, char* argv[]) {
    using namespace rhythm;
    try {
        Require(argc == 4 || argc == 5, "export_gpu_tests package music output [frames]");
        const auto frames = argc == 5 ? std::stoi(argv[4]) : 120;
        Require(frames == 120 || frames == 480, "unsupported export validation duration");
        const auto samples = static_cast<std::size_t>(frames) * 1600;
        std::cout << std::unitbuf;
        const auto package = project::LoadPackage(argv[1]);
        const auto root =
                std::filesystem::path(argv[3]) /
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        std::filesystem::create_directories(root);
        const auto source_audio = ReviewAudio(argv[2], samples, true);
        exporting::ExportSettings settings;
        settings.encoding_ = {640, 360, 30, 8000000, media::VideoCodec::kMpeg4, true};
        settings.frames_ = frames;
        settings.music_ = argv[2];
        platform::Host host(true);
        std::vector<std::uint64_t> reference;
        for (int scenario = 0; scenario < 3; ++scenario) {
            const auto path = root / (scenario == 0   ? "music.mp4"
                                      : scenario == 1 ? "repeat.mp4"
                                                      : "silence.mp4");
            if (scenario == 2) settings.music_.reset();
            {
                auto renderer = host.CreateRenderer();
                std::uint64_t next = 0;
                exporting::RenderExport(
                        package, settings, path, renderer, [&](exporting::ExportProgress progress) {
                            Require(progress.completed_frames_ == next++, "offline progress order");
                            Require(progress.total_frames_ == frames &&
                                            progress.texture_bytes_ <= 256ULL * 1024 * 1024,
                                    "offline budget or progress total");
                        });
                Require(next == frames + 1 && renderer.Stats().texture_bytes_ == 0,
                        "completed export retains graph or staging resources");
            }
            const auto hashes = ReviewVideo(path, frames);
            if (scenario == 0) reference = hashes;
            if (scenario == 1) Require(hashes == reference, "offline render is not deterministic");
            if (scenario == 2)
                Require(hashes != reference, "soundtrack did not drive the exported graph");
            const auto audio = ReviewAudio(path, samples);
            double squared_error = 0;
            for (std::size_t index = 0; index < audio.size(); ++index) {
                const auto expected = scenario == 2 ? 0.0f : source_audio[index];
                squared_error += std::pow(audio[index] - expected, 2);
            }
            Require(squared_error / audio.size() < 0.002,
                    "offline soundtrack timing or samples changed");
            std::cout << path.string() << " audio_mse=" << squared_error / audio.size() << '\n';
        }
        {
            auto renderer = host.CreateRenderer();
            std::stop_source cancel;
            bool canceled = false;
            std::uint64_t completed = 0;
            try {
                exporting::RenderExport(
                        package, settings, root / "canceled.mp4", renderer,
                        [&](exporting::ExportProgress progress) {
                            completed = progress.completed_frames_;
                            if (progress.completed_frames_ == 10) cancel.request_stop();
                        },
                        cancel.get_token());
            } catch (const std::exception& error) {
                canceled = std::string_view(error.what()) == "export.canceled";
            }
            Require(canceled && completed == 10, "offline cancellation was ignored");
        }
        std::cout << "offline graph + real music + async readback + MP4 round-trip passed: "
                  << root.string() << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
