#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <stop_token>

namespace rhythm::media {
enum class VideoCodec { kMpeg4, kH264 };
struct EncodingSettings {
    std::uint32_t width_ = 1280;
    std::uint32_t height_ = 720;
    std::uint32_t fps_ = 30;
    std::uint32_t bitrate_ = 8000000;
    VideoCodec codec_ = VideoCodec::kMpeg4;
    bool audio_ = true;
};
// Serialized worker-only encoding into a caller-owned staging file. Video is
// top-left premultiplied RGBA8, written over black; audio is interleaved stereo float/48 kHz.
// Input timestamps derive from sequential frame/sample counts. Audio/video must
// stay within one second of each other and end at the same requested duration.
// Finish drains codecs and writes the MP4 trailer; destruction never publishes
// a partial file. Callers own atomic publication/removal of their staging files.
class AvWriter final {
   public:
    AvWriter(const std::filesystem::path& staging, EncodingSettings settings,
             std::stop_token stop = {});
    ~AvWriter();
    AvWriter(const AvWriter&) = delete;
    AvWriter& operator=(const AvWriter&) = delete;
    void WriteVideo(std::span<const std::uint8_t> rgba);
    void WriteAudio(std::span<const float> stereo);
    void Finish();

   private:
    class Impl;
    std::unique_ptr<Impl> impl_{};
};
}  // namespace rhythm::media
