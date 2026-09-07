#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <vector>

#include "rhythm/media/video_frame.h"

namespace rhythm::media {
// Serialized worker ownership, local regular files only. At most one decoded
// frame is retained internally; callers must bound their own output queue.
// Read performs I/O/conversion, never call it on UI/render/device threads.
class VideoDecoder final {
   public:
    explicit VideoDecoder(const std::filesystem::path& path, std::uint64_t generation = 1,
                          std::stop_token stop = {});
    // Copies at most 16 MiB on the calling worker. Embedded media uses the same
    // FFmpeg path; no temporary file, network protocol or borrowed storage.
    explicit VideoDecoder(std::span<const std::uint8_t> bytes, std::uint64_t generation = 1,
                          std::stop_token stop = {});
    ~VideoDecoder();
    VideoDecoder(VideoDecoder&&) noexcept;
    VideoDecoder& operator=(VideoDecoder&&) noexcept;
    VideoDecoder(const VideoDecoder&) = delete;
    VideoDecoder& operator=(const VideoDecoder&) = delete;
    VideoInfo Info() const;
    std::optional<VideoFrame> Read(std::stop_token stop = {});
    // Exact timestamp selection currently replays from the start with bounded
    // memory and cancellation. Next Read returns the first PTS >= seconds.
    // Failed/canceled seek leaves the previous decoder intact.
    void Seek(double seconds, std::uint64_t generation, std::stop_token stop = {});

   private:
    class Impl;
    std::unique_ptr<Impl> impl_{};
};
}  // namespace rhythm::media
