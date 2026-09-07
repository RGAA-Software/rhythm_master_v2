#pragma once

#include <filesystem>
#include <fstream>
#include <stop_token>

#include "ffmpeg_resources.h"

namespace rhythm::media::detail {
// Nonmovable custom local I/O, adapted from LocalInput. The caller owns the
// staging directory; this adapter never opens a FFmpeg file/network protocol.
class LocalOutput final {
   public:
    LocalOutput(const std::filesystem::path& path, std::stop_token stop);
    LocalOutput(const LocalOutput&) = delete;
    LocalOutput& operator=(const LocalOutput&) = delete;
    AVIOContext& Context() { return *context_; }
    void Flush();
    bool Canceled() const { return stop_.stop_requested(); }

   private:
    static int Write(void* opaque, std::uint8_t* buffer, int size) noexcept;
    static std::int64_t Seek(void* opaque, std::int64_t offset, int whence) noexcept;
    std::ofstream file_{};
    std::int64_t position_ = 0;
    std::int64_t size_ = 0;
    std::stop_token stop_{};
    std::unique_ptr<AVIOContext, IoDelete> context_{};
};
}  // namespace rhythm::media::detail
