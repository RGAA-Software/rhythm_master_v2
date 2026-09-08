#include "import_file.h"

#include <utility>

namespace rhythm::android_host {
std::optional<ImportFile> ImportFile::Claim(const std::filesystem::path& path,
                                            const std::filesystem::path& canonical_cache) {
    if (std::filesystem::symlink_status(path).type() != std::filesystem::file_type::regular ||
        std::filesystem::canonical(path.parent_path()) != canonical_cache ||
        !path.filename().string().starts_with("incoming-") || path.extension() != ".rhythmpack")
        return {};
    return ImportFile(canonical_cache / path.filename());
}
ImportFile::~ImportFile() {
    if (!path_.empty()) {
        std::error_code error;
        std::filesystem::remove(path_, error);
    }
}
ImportFile::ImportFile(ImportFile&& other) noexcept : path_(std::exchange(other.path_, {})) {}
}  // namespace rhythm::android_host
