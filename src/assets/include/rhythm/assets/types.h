#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>

namespace rhythm::assets {
struct AssetId {
    std::string sha256_{};
    bool operator==(const AssetId&) const = default;
};
struct AssetRecord {
    AssetId id_{};
    std::uint64_t bytes_ = 0;
    std::string media_type_{};
    bool operator==(const AssetRecord&) const = default;
};
inline bool ValidId(const AssetId& id) {
    return id.sha256_.size() == 64 &&
           std::all_of(id.sha256_.begin(), id.sha256_.end(), [](char value) {
               return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f');
           });
}
inline bool ValidMediaType(std::string_view media_type) {
    const auto slash = media_type.find('/');
    return !media_type.empty() && media_type.size() <= 128 && slash != 0 &&
           slash != std::string_view::npos && slash + 1 < media_type.size() &&
           std::all_of(media_type.begin(), media_type.end(),
                       [](char value) { return value >= '!' && value <= '~'; });
}
}  // namespace rhythm::assets
