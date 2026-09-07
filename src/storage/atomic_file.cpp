#include "rhythm/storage/atomic_file.h"

#include <fstream>
#include <memory>
#include <stdexcept>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#ifdef __linux__
#include <linux/fs.h>
#include <sys/syscall.h>
#endif
#endif

namespace rhythm::storage {
namespace {
#ifdef _WIN32
struct HandleCloser {
    void operator()(void* handle) const { CloseHandle(handle); }
};
void Flush(const std::filesystem::path& path) {
    // CreateFile transfers exclusive HANDLE ownership to this narrow RAII adapter.
    const auto native = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (native == INVALID_HANDLE_VALUE) throw std::runtime_error("project.flush_open");
    const std::unique_ptr<void, HandleCloser> handle(native);
    if (!FlushFileBuffers(handle.get())) throw std::runtime_error("project.flush");
}
#else
class FileDescriptor final {
   public:
    explicit FileDescriptor(const std::filesystem::path& path)
        : descriptor_(open(path.c_str(), O_RDONLY)) {
        if (descriptor_ < 0) throw std::runtime_error("project.flush_open");
    }
    ~FileDescriptor() { close(descriptor_); }
    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;
    void Flush() const {
        if (fsync(descriptor_) != 0) throw std::runtime_error("project.flush");
    }

   private:
    int descriptor_ = -1;
};
void Flush(const std::filesystem::path& path) {
    FileDescriptor file(path);
    file.Flush();
}
#endif
}  // namespace
class WriteGuard::Impl final {
   public:
    explicit Impl(const std::filesystem::path& project) {
        const auto path = project / ".writer";
#ifdef _WIN32
        const auto native = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                                        OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (native == INVALID_HANDLE_VALUE) throw std::runtime_error("project.writer_busy");
        handle_.reset(native);
#else
        descriptor_ = open(path.c_str(), O_CREAT | O_RDWR, 0600);
        if (descriptor_ < 0) throw std::runtime_error("project.writer_open");
        if (flock(descriptor_, LOCK_EX | LOCK_NB) != 0) {
            close(descriptor_);
            descriptor_ = -1;
            throw std::runtime_error("project.writer_busy");
        }
#endif
    }
    ~Impl() {
#ifndef _WIN32
        if (descriptor_ >= 0) close(descriptor_);
#endif
    }

   private:
#ifdef _WIN32
    std::unique_ptr<void, HandleCloser> handle_{};
#else
    int descriptor_ = -1;
#endif
};
WriteGuard::WriteGuard(const std::filesystem::path& project)
    : impl_(std::make_unique<Impl>(project)) {}
WriteGuard::~WriteGuard() = default;
void SyncFile(const std::filesystem::path& path) { Flush(path); }
void WriteDurable(const std::filesystem::path& path, std::string_view bytes) {
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file.exceptions(std::ios::badbit | std::ios::failbit);
        file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        file.flush();
    }
    Flush(path);
}
void Replace(const std::filesystem::path& source, const std::filesystem::path& destination) {
#ifdef _WIN32
    if (!MoveFileExW(source.c_str(), destination.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("project.commit");
#else
    std::filesystem::rename(source, destination);
    Flush(destination.parent_path());
#endif
}
void SyncDirectory(const std::filesystem::path& path) {
#ifdef _WIN32
    // Windows uses file flushes and a write-through pointer replacement. The
    // POSIX directory fsync guarantee is not claimed for this backend.
    static_cast<void>(path);
#else
    Flush(path);
#endif
}
void PublishNew(const std::filesystem::path& source, const std::filesystem::path& destination) {
    SyncFile(source);
#ifdef _WIN32
    if (!MoveFileExW(source.c_str(), destination.c_str(), MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("export.destination_exists_or_unwritable");
#elif defined(__linux__)
    // Android app/shell policies can deny hard links. The kernel's atomic
    // no-replace rename preserves the same contract without creating a link.
    // Use syscall because bionic exposes renameat2 only from API 30; our floor
    // is API 26. An unsupported kernel/filesystem fails without a racy fallback.
    if (syscall(SYS_renameat2, AT_FDCWD, source.c_str(), AT_FDCWD, destination.c_str(),
                RENAME_NOREPLACE) != 0)
        throw std::runtime_error("export.destination_exists_or_unwritable");
    SyncDirectory(destination.parent_path());
#else
    // link is atomic and fails with EEXIST; unlike rename it cannot clobber a
    // destination created by another process between checking and committing.
    std::filesystem::create_hard_link(source, destination);
    std::error_code ignored;
    std::filesystem::remove(source, ignored);
    SyncDirectory(destination.parent_path());
#endif
}
}  // namespace rhythm::storage
