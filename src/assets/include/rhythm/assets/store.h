#pragma once

#include <filesystem>
#include <stop_token>
#include <string>

#include "rhythm/assets/types.h"

namespace rhythm::assets {
// Blocking I/O belongs on a bounded worker, never a render/audio callback.
// Blobs are immutable; original machine paths are not part of asset identity.
class Store final {
   public:
    explicit Store(std::filesystem::path directory);
    AssetRecord Import(const std::filesystem::path& source, std::string media_type,
                       std::uint64_t maximum_bytes = 256 * 1024 * 1024,
                       std::stop_token cancellation = {});
    [[nodiscard]] bool Verify(const AssetRecord& asset) const;
    AssetRecord CopyFrom(const Store& source, const AssetRecord& asset,
                         std::uint64_t maximum_bytes = 256 * 1024 * 1024,
                         std::stop_token cancellation = {});
    std::string Read(const AssetRecord& asset,
                     std::uint64_t maximum_bytes = 32 * 1024 * 1024) const;

   private:
    std::filesystem::path BlobPath(const AssetId& id) const;
    std::filesystem::path directory_{};
};
}  // namespace rhythm::assets
