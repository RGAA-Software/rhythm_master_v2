#pragma once

#include <filesystem>
#include <optional>
#include <stop_token>
#include <string>

#include "rhythm/assets/types.h"

namespace rhythm::storage {
class FileBytes;
}

namespace rhythm::assets {
enum class AssetHealth { kValid, kMissing, kCorrupt };
struct AssetCheck {
    AssetRecord asset_{};
    AssetHealth health_ = AssetHealth::kMissing;
};
// Worker-only hash validation over a bounded open-file range.
bool VerifyFile(const AssetRecord& asset, const storage::FileBytes& bytes,
                std::stop_token cancellation = {});
// Blocking I/O belongs on a bounded worker, never a render/audio callback.
// Blobs are immutable; original machine paths are not part of asset identity.
class Store final {
   public:
    explicit Store(std::filesystem::path directory);
    AssetRecord Import(const std::filesystem::path& source, std::string media_type,
                       std::uint64_t maximum_bytes = 256 * 1024 * 1024,
                       std::stop_token cancellation = {});
    // Exact-content repair: mismatched bytes never replace the requested blob.
    // Restoring a missing/corrupt copy does not change the authored asset ID.
    void Restore(const std::filesystem::path& source, const AssetRecord& expected,
                 std::stop_token cancellation = {});
    AssetHealth Inspect(const AssetRecord& asset, std::stop_token cancellation = {}) const;
    [[nodiscard]] bool Verify(const AssetRecord& asset) const;
    storage::FileBytes Open(const AssetRecord& asset,
                            std::uint64_t maximum_bytes = 256 * 1024 * 1024,
                            std::stop_token cancellation = {}) const;
    AssetRecord CopyFrom(const Store& source, const AssetRecord& asset,
                         std::uint64_t maximum_bytes = 256 * 1024 * 1024,
                         std::stop_token cancellation = {});
    std::string Read(const AssetRecord& asset,
                     std::uint64_t maximum_bytes = 32 * 1024 * 1024) const;

   private:
    AssetRecord ImportChecked(const std::filesystem::path& source, std::string media_type,
                              std::uint64_t maximum_bytes, std::stop_token cancellation,
                              const std::optional<AssetRecord>& expected);
    std::filesystem::path BlobPath(const AssetId& id) const;
    std::filesystem::path directory_{};
};
}  // namespace rhythm::assets
