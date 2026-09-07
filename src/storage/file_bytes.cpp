#include "rhythm/storage/file_bytes.h"

#include <algorithm>
#include <cstdio>
#include <limits>
#include <mutex>
#include <stdexcept>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace rhythm::storage {
namespace {
#ifdef _WIN32
// Reuse atomic_storage's native HANDLE ownership. Sharing deletion permits
// publishing a replacement while a decoder continues reading the old file.
struct HandleClose {
    void operator()(void* handle) const { CloseHandle(handle); }
};
#else
// Focused adaptation of first-party px_common/file.cpp's binary file ownership
// and 64-bit seek/tell boundary. See provenance/file_bytes.json.
struct FileClose {
    void operator()(std::FILE* file) const { std::fclose(file); }
};
using FileHandle = std::unique_ptr<std::FILE, FileClose>;
void Seek(const FileHandle& file, std::uint64_t offset, int origin) {
    if (offset > static_cast<std::uint64_t>(std::numeric_limits<off_t>::max()) ||
        fseeko(file.get(), static_cast<off_t>(offset), origin))
        throw std::runtime_error("storage.file_seek");
}
std::uint64_t Tell(const FileHandle& file) {
    const auto offset = ftello(file.get());
    if (offset < 0) throw std::runtime_error("storage.file_tell");
    return static_cast<std::uint64_t>(offset);
}
#endif
}  // namespace
class FileBytes::Impl final {
   public:
    Impl(const std::filesystem::path& path, std::uint64_t maximum_bytes) {
        if (!std::filesystem::is_regular_file(path))
            throw std::invalid_argument("storage.file_type");
#ifdef _WIN32
        const auto opened =
                CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE,
                            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (opened == INVALID_HANDLE_VALUE) throw std::runtime_error("storage.file_open");
        handle_.reset(opened);
        LARGE_INTEGER size{};
        if (!GetFileSizeEx(handle_.get(), &size) || size.QuadPart < 0)
            throw std::runtime_error("storage.file_size");
        size_ = static_cast<std::uint64_t>(size.QuadPart);
#else
        file_.reset(std::fopen(path.c_str(), "rb"));
        if (!file_) throw std::runtime_error("storage.file_open");
        Seek(file_, 0, SEEK_END);
        size_ = Tell(file_);
#endif
        if (size_ > maximum_bytes) throw std::length_error("storage.file_budget");
    }
    std::uint64_t Size() const { return size_; }
    void Read(std::uint64_t offset, std::span<std::uint8_t> destination) const {
        const std::lock_guard lock(mutex_);
#ifdef _WIN32
        LARGE_INTEGER position{};
        position.QuadPart = static_cast<std::int64_t>(offset);
        if (!SetFilePointerEx(handle_.get(), position, nullptr, FILE_BEGIN))
            throw std::runtime_error("storage.file_seek");
        while (!destination.empty()) {
            const auto count = static_cast<DWORD>(
                    std::min<std::size_t>(destination.size(), std::numeric_limits<DWORD>::max()));
            DWORD read = 0;
            if (!ReadFile(handle_.get(), destination.data(), count, &read, nullptr) ||
                read != count)
                throw std::runtime_error("storage.file_changed_or_read_failed");
            destination = destination.subspan(read);
        }
#else
        Seek(file_, offset, SEEK_SET);
        if (std::fread(destination.data(), 1, destination.size(), file_.get()) !=
            destination.size())
            throw std::runtime_error("storage.file_changed_or_read_failed");
#endif
    }

   private:
    // Exclusive native ownership remains inside this synchronous adapter.
#ifdef _WIN32
    std::unique_ptr<void, HandleClose> handle_{};
#else
    FileHandle file_{};
#endif
    mutable std::mutex mutex_{};
    std::uint64_t size_ = 0;
};
FileBytes FileBytes::Open(const std::filesystem::path& path, std::uint64_t maximum_bytes) {
    FileBytes result;
    result.impl_ = std::make_shared<Impl>(path, maximum_bytes);
    result.size_ = result.impl_->Size();
    return result;
}
FileBytes FileBytes::Slice(std::uint64_t offset, std::uint64_t bytes) const {
    if (!impl_ || offset > size_ || bytes > size_ - offset)
        throw std::out_of_range("storage.file_range");
    auto result = *this;
    result.offset_ += offset;
    result.size_ = bytes;
    return result;
}
std::size_t FileBytes::Read(std::uint64_t offset, std::span<std::uint8_t> destination) const {
    if (!impl_ || offset > size_) throw std::out_of_range("storage.file_range");
    const auto count =
            static_cast<std::size_t>(std::min<std::uint64_t>(size_ - offset, destination.size()));
    if (count) impl_->Read(offset_ + offset, destination.first(count));
    return count;
}
}  // namespace rhythm::storage
