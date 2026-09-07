#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace rhythm::qr {
struct Image {
    std::uint32_t width_ = 0;
    std::uint32_t symbol_modules_ = 0;
    std::uint32_t pixels_per_module_ = 0;
    std::uint32_t quiet_modules_ = 4;
    std::vector<std::uint8_t> rgba_{};
};
// Opaque payload bytes, 1..1024 bytes, never logged. Produces an opaque square
// with a four-module white quiet zone. Maximum extent is 64..2048 pixels; the
// actual extent fits whole modules, at least two pixels each, or throws.
// Medium error correction or higher; no network invitation/security semantics.
Image Generate(std::string_view payload, std::uint32_t maximum_extent = 512);
}  // namespace rhythm::qr
