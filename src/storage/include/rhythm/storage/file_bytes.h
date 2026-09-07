#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>

namespace rhythm::storage {
// Shared ownership of an open local regular file and an immutable range within
// it. Copies/slices keep the handle alive and have no shared caller-side cursor.
// Read performs serialized blocking I/O into caller-owned memory: worker use
// only. The publisher must keep content immutable (replace files atomically).
// This is an open-file lease, not a snapshot of arbitrary in-place file writes.
class FileBytes final {
   public:
    FileBytes() = default;
    static FileBytes Open(const std::filesystem::path& path, std::uint64_t maximum_bytes);
    bool Valid() const { return bool(impl_); }
    std::uint64_t Size() const { return size_; }
    FileBytes Slice(std::uint64_t offset, std::uint64_t bytes) const;
    // EOF returns zero. A short underlying file is an error, never silent EOF
    // within a previously validated range. Invalid ranges throw before I/O.
    std::size_t Read(std::uint64_t offset, std::span<std::uint8_t> destination) const;

   private:
    class Impl;
    std::shared_ptr<const Impl> impl_{};
    std::uint64_t offset_ = 0;
    std::uint64_t size_ = 0;
};
}  // namespace rhythm::storage
