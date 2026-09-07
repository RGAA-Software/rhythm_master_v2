#pragma once
#include <filesystem>
#include <memory>
#include <string_view>
namespace rhythm::storage {
void SyncDirectory(const std::filesystem::path& path);
void SyncFile(const std::filesystem::path& path);
class WriteGuard final {
   public:
    explicit WriteGuard(const std::filesystem::path& project);
    ~WriteGuard();
    WriteGuard(const WriteGuard&) = delete;
    WriteGuard& operator=(const WriteGuard&) = delete;

   private:
    class Impl;
    std::unique_ptr<Impl> impl_{};
};
void WriteDurable(const std::filesystem::path& path, std::string_view bytes);
void Replace(const std::filesystem::path& source, const std::filesystem::path& destination);
// Atomically install a completed file without replacing an existing destination.
// Source and destination must reside on the same filesystem.
void PublishNew(const std::filesystem::path& source, const std::filesystem::path& destination);
}  // namespace rhythm::storage
