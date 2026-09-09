#pragma once

#include <vector>

#include "rhythm/assets/types.h"

namespace rhythm::assets {
// Prepared off-thread and published immutably. IDs identify the encoded source;
// pixels/metadata for an ID cannot change within a running program.
struct ImageResource {
    AssetId id_{};
    std::uint16_t width_ = 0;
    std::uint16_t height_ = 0;
    double pixel_aspect_ = 1;
    double clockwise_rotation_ = 0;
    std::vector<std::uint8_t> rgba_{};
    // Empty for decoded images; exact layout identity for a font-derived mask.
    std::string variant_key_{};
    std::uint32_t missing_glyphs_ = 0;
    bool clipped_ = false;
};
struct Images {
    std::vector<ImageResource> images_{};
};
inline constexpr std::size_t kMaximumImageBytes = 64 * 1024 * 1024;
}  // namespace rhythm::assets
