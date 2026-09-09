#include "rhythm/text/rasterizer.h"

#include <ft2build.h>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <map>
#include <stdexcept>
#include <type_traits>
#include FT_FREETYPE_H
#include <utf8.h>

namespace rhythm::text {
namespace {
constexpr std::size_t kMaximumGlyphs = 2048;
constexpr std::size_t kMaximumCacheBytes = 8 * 1024 * 1024;
struct LibraryDelete {
    void operator()(FT_Library value) const { FT_Done_FreeType(value); }
};
struct FaceDelete {
    void operator()(FT_Face value) const { FT_Done_Face(value); }
};
struct Glyph {
    int left_ = 0;
    int top_ = 0;
    int advance_ = 0;
    unsigned width_ = 0;
    unsigned height_ = 0;
    std::vector<std::uint8_t> pixels_{};
};
struct Placement {
    std::uint32_t index_ = 0;
    int x_ = 0;
    std::size_t line_ = 0;
};
}  // namespace
struct Rasterizer::Impl {
    // FreeType creates these opaque objects. The RAII deleters are their sole
    // owners. Font bytes outlive the face, and the face outlives no library.
    std::vector<std::uint8_t> font_{};
    std::unique_ptr<std::remove_pointer_t<FT_Library>, LibraryDelete> library_{};
    std::unique_ptr<std::remove_pointer_t<FT_Face>, FaceDelete> face_{};
    std::map<std::pair<unsigned, unsigned>, Glyph> glyphs_{};
    CacheStats stats_{};

    explicit Impl(std::span<const std::uint8_t> font) {
        if (font.empty() || font.size() > 32 * 1024 * 1024)
            throw std::invalid_argument("text.font_budget");
        font_.assign(font.begin(), font.end());
        FT_Library library = nullptr;
        if (FT_Init_FreeType(&library)) throw std::runtime_error("text.library");
        library_.reset(library);
        FT_Face face = nullptr;
        if (FT_New_Memory_Face(library_.get(), font_.data(), static_cast<FT_Long>(font_.size()), 0,
                               &face))
            throw std::invalid_argument("text.invalid_font");
        face_.reset(face);
        if (FT_Select_Charmap(face_.get(), FT_ENCODING_UNICODE))
            throw std::invalid_argument("text.font_unicode");
    }
    const Glyph& Get(unsigned index, unsigned size) {
        const auto key = std::pair{index, size};
        if (const auto found = glyphs_.find(key); found != glyphs_.end()) return found->second;
        if (FT_Load_Glyph(face_.get(), index, FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL))
            throw std::runtime_error("text.glyph");
        const auto& slot = *face_->glyph;
        const auto& bitmap = slot.bitmap;
        if (bitmap.pixel_mode != FT_PIXEL_MODE_GRAY || bitmap.num_grays != 256)
            throw std::invalid_argument("text.glyph_format");
        const auto bytes = std::size_t(bitmap.width) * bitmap.rows;
        if (bytes > kMaximumCacheBytes) throw std::length_error("text.glyph_budget");
        if (glyphs_.size() >= kMaximumGlyphs || bytes > kMaximumCacheBytes - stats_.bytes_) {
            glyphs_.clear();
            stats_.bytes_ = 0;
        }
        Glyph glyph{slot.bitmap_left, slot.bitmap_top, static_cast<int>(slot.advance.x / 64),
                    bitmap.width, bitmap.rows};
        glyph.pixels_.resize(bytes);
        for (unsigned y = 0; y < bitmap.rows; ++y) {
            const auto row = bitmap.buffer + static_cast<std::ptrdiff_t>(y) * bitmap.pitch;
            std::copy_n(row, bitmap.width, glyph.pixels_.begin() + std::size_t(y) * bitmap.width);
        }
        stats_.bytes_ += bytes;
        ++stats_.rasterizations_;
        const auto inserted = glyphs_.emplace(key, std::move(glyph));
        stats_.glyphs_ = glyphs_.size();
        return inserted.first->second;
    }
};

Rasterizer::Rasterizer(std::span<const std::uint8_t> font) : impl_(std::make_unique<Impl>(font)) {}
Rasterizer::~Rasterizer() = default;
CacheStats Rasterizer::Stats() const { return impl_->stats_; }

Mask Rasterizer::Render(const Layout& layout) {
    if (!layout.width_ || !layout.height_ || layout.width_ > 2048 || layout.height_ > 2048 ||
        layout.pixel_size_ < 8 || layout.pixel_size_ > 256 || layout.text_.size() > 16384 ||
        !std::isfinite(layout.line_spacing_) || layout.line_spacing_ < .5f ||
        layout.line_spacing_ > 4 ||
        (layout.alignment_ != Alignment::kLeft && layout.alignment_ != Alignment::kCenter &&
         layout.alignment_ != Alignment::kRight))
        throw std::invalid_argument("text.layout_budget");
    std::vector<std::uint32_t> codepoints;
    try {
        utf8::utf8to32(layout.text_.begin(), layout.text_.end(), std::back_inserter(codepoints));
    } catch (const utf8::exception&) {
        throw std::invalid_argument("text.invalid_utf8");
    }
    if (codepoints.size() > 4096) throw std::length_error("text.character_budget");
    if (FT_Set_Pixel_Sizes(impl_->face_.get(), 0, layout.pixel_size_))
        throw std::invalid_argument("text.font_size");
    Mask result{layout.width_, layout.height_};
    result.coverage_.resize(std::size_t(layout.width_) * layout.height_);
    std::vector<Placement> placements;
    std::vector<int> widths{0};
    unsigned previous = 0;
    for (const auto codepoint : codepoints) {
        if (codepoint == '\r') continue;
        if (codepoint == '\n') {
            widths.push_back(0);
            previous = 0;
            continue;
        }
        if (codepoint < 32 || codepoint == 127)
            throw std::invalid_argument("text.control_character");
        auto index = FT_Get_Char_Index(impl_->face_.get(), codepoint);
        if (!index) {
            ++result.missing_glyphs_;
            index = FT_Get_Char_Index(impl_->face_.get(), 0xfffd);
        }
        const auto advance = impl_->Get(index, layout.pixel_size_).advance_;
        FT_Vector kerning{};
        if (previous && index && FT_HAS_KERNING(impl_->face_.get()))
            if (FT_Get_Kerning(impl_->face_.get(), previous, index, FT_KERNING_DEFAULT, &kerning))
                throw std::runtime_error("text.kerning");
        auto x = widths.back() + static_cast<int>(kerning.x / 64);
        if (layout.wrap_ && widths.back() > 0 && x + advance > static_cast<int>(layout.width_)) {
            widths.push_back(0);
            x = 0;
        }
        placements.push_back({index, x, widths.size() - 1});
        widths.back() = x + advance;
        previous = index;
    }
    const auto baseline = static_cast<int>(impl_->face_->size->metrics.ascender / 64);
    const auto line_height = static_cast<int>(std::ceil(layout.pixel_size_ * layout.line_spacing_));
    std::size_t raster_work = 0;
    for (const auto& placement : placements) {
        const auto& glyph = impl_->Get(placement.index_, layout.pixel_size_);
        raster_work += glyph.pixels_.size();
        if (raster_work > 16 * 1024 * 1024) throw std::length_error("text.raster_budget");
        const auto remaining = static_cast<int>(layout.width_) - widths[placement.line_];
        const auto shift = layout.alignment_ == Alignment::kCenter  ? remaining / 2
                           : layout.alignment_ == Alignment::kRight ? remaining
                                                                    : 0;
        const auto left = placement.x_ + shift + glyph.left_;
        const auto top = baseline + static_cast<int>(placement.line_) * line_height - glyph.top_;
        for (unsigned y = 0; y < glyph.height_; ++y)
            for (unsigned x = 0; x < glyph.width_; ++x) {
                const auto coverage = glyph.pixels_[std::size_t(y) * glyph.width_ + x];
                if (!coverage) continue;
                const auto target_x = left + static_cast<int>(x);
                const auto target_y = top + static_cast<int>(y);
                if (target_x < 0 || target_y < 0 || target_x >= static_cast<int>(layout.width_) ||
                    target_y >= static_cast<int>(layout.height_)) {
                    result.clipped_ = true;
                    continue;
                }
                auto& destination =
                        result.coverage_[std::size_t(target_y) * layout.width_ + target_x];
                destination = static_cast<std::uint8_t>(
                        coverage + (destination * (255 - coverage) + 127) / 255);
            }
    }
    result.glyphs_ = static_cast<std::uint32_t>(placements.size());
    result.lines_ = static_cast<std::uint32_t>(widths.size());
    return result;
}
}  // namespace rhythm::text
