#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace rhythm::text {
enum class Alignment { kLeft, kCenter, kRight };
struct Layout {
    std::string text_{};
    std::uint32_t width_ = 512;
    std::uint32_t height_ = 256;
    std::uint32_t pixel_size_ = 48;
    float line_spacing_ = 1.2f;
    Alignment alignment_ = Alignment::kLeft;
    bool wrap_ = true;
};
struct Mask {
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    std::vector<std::uint8_t> coverage_{};
    std::uint32_t glyphs_ = 0;
    std::uint32_t missing_glyphs_ = 0;
    std::uint32_t lines_ = 0;
    bool clipped_ = false;
};
struct CacheStats {
    std::size_t glyphs_ = 0;
    std::size_t bytes_ = 0;
    std::uint64_t rasterizations_ = 0;
};
// Owns immutable font bytes and a bounded glyph cache. Use on one worker thread;
// publish the resulting value mask, never this mutable cache, to the renderer.
// This profile supports horizontal CJK/Latin, codepoint wrapping and FreeType
// kerning. It does not claim bidi, complex-script shaping or grapheme editing.
class Rasterizer final {
   public:
    explicit Rasterizer(std::span<const std::uint8_t> font);
    ~Rasterizer();
    Rasterizer(const Rasterizer&) = delete;
    Rasterizer& operator=(const Rasterizer&) = delete;
    Mask Render(const Layout& layout);
    CacheStats Stats() const;

   private:
    struct Impl;
    std::unique_ptr<Impl> impl_{};
};
}  // namespace rhythm::text
