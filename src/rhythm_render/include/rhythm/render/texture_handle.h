#pragma once

#include <cstdint>

namespace rhythm::render {
// Non-owning stable value identity. The device validates generation and usage.
struct TextureHandle {
    std::uint64_t device_ = 0;
    std::uint32_t slot_ = 0;
    std::uint32_t generation_ = 0;
    bool operator==(const TextureHandle&) const = default;
};
}  // namespace rhythm::render
