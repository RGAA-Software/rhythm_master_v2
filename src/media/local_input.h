#pragma once

#include <filesystem>
#include <fstream>
#include <memory>
#include <stop_token>
#include <vector>

#include "ffmpeg_resources.h"
#include "rhythm/storage/file_bytes.h"

namespace rhythm::media::detail {
// Nonmovable because the C I/O callback borrows this object's address. This
// object outlives its format context; no FFmpeg file/network protocol is opened.
class LocalInput final {
   public:
    LocalInput(const std::filesystem::path& path, std::stop_token stop);
    LocalInput(std::shared_ptr<const std::vector<std::uint8_t>> bytes, std::stop_token stop);
    LocalInput(storage::FileBytes bytes, std::stop_token stop);
    LocalInput(const LocalInput&) = delete;
    LocalInput& operator=(const LocalInput&) = delete;
    AVIOContext& Context() { return *context_; }
    void SetStop(std::stop_token stop) { stop_ = stop; }
    bool Canceled() const { return stop_.stop_requested(); }

   private:
    void InitializeContext();
    static int Read(void* opaque, std::uint8_t* buffer, int size) noexcept;
    static std::int64_t Seek(void* opaque, std::int64_t offset, int whence) noexcept;
    std::ifstream file_{};
    std::shared_ptr<const std::vector<std::uint8_t>> bytes_{};
    storage::FileBytes file_bytes_{};
    std::int64_t position_ = 0;
    std::int64_t size_ = 0;
    std::stop_token stop_{};
    std::unique_ptr<AVIOContext, IoDelete> context_{};
};
}  // namespace rhythm::media::detail
