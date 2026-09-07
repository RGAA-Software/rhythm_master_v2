#pragma once

#include <optional>
#include <stop_token>

#include "package_archive.h"
#include "rhythm/project/package.h"
#include "rhythm/storage/file_bytes.h"

namespace rhythm::project::detail {
inline constexpr std::uint64_t kMaximumStreamedMusicBytes = kMaximumMusicAssetBytes;
inline constexpr std::uint64_t kMaximumFileArchiveBytes = kMaximumFilePackageBytes;
struct ArchiveMedia {
    std::string name_{};
    storage::FileBytes bytes_{};
    std::string sha256_{};
};
struct FileArchive {
    PackageEntries entries_{};
    std::optional<ArchiveMedia> media_{};
    std::size_t native_peak_bytes_ = 0;
};
// Worker-only. Native CRC and incremental SHA-256 validate the media before its
// bounded range is returned. Other entries retain the small package budgets.
FileArchive ReadFileArchive(storage::FileBytes source, std::stop_token stop = {});
// Writes a caller-owned staging path. Caller validates and atomically publishes;
// errors/cancellation never authorize publishing a partially written archive.
void WriteFileArchive(const std::filesystem::path& path, const PackageEntries& entries,
                      const ArchiveMedia& media, std::stop_token stop = {});
}  // namespace rhythm::project::detail
