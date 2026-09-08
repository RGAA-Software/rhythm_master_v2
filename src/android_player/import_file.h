#pragma once

#include <filesystem>
#include <optional>

namespace rhythm::android_host {
// Move-only ownership of a checked app-private incoming package copy. Never
// accepts provider originals or symlinks. Workers must stop before destruction.
class ImportFile final {
   public:
    static std::optional<ImportFile> Claim(const std::filesystem::path& path,
                                           const std::filesystem::path& canonical_cache);
    ~ImportFile();
    ImportFile(ImportFile&& other) noexcept;
    ImportFile(const ImportFile&) = delete;
    ImportFile& operator=(const ImportFile&) = delete;
    const std::filesystem::path& Path() const { return path_; }

   private:
    explicit ImportFile(std::filesystem::path path) : path_(std::move(path)) {}
    std::filesystem::path path_{};
};
}  // namespace rhythm::android_host
