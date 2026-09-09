#pragma once

#include <map>

#include "rhythm/prepared_assets/prepare.h"
#include "rhythm/text/rasterizer.h"

namespace rhythm::prepared_assets::detail {
// One preparation worker owns this cache. At most two fonts retain at most
// 64 MiB of font bytes plus 16 MiB of bounded Rasterizer glyph storage.
class TextCache final {
   public:
    text::Mask Render(const project::PackagedAsset& font, const text::Layout& layout);
    std::size_t FontCount() const { return fonts_.size(); }
    std::uint64_t Rasterizations() const { return rasterizations_; }
    std::uint64_t LayoutRenders() const { return layout_renders_; }

   private:
    struct Entry {
        std::unique_ptr<text::Rasterizer> font_{};
        std::uint64_t used_ = 0;
    };
    std::map<std::string, Entry> fonts_{};
    std::uint64_t sequence_ = 0;
    std::uint64_t rasterizations_ = 0;
    std::uint64_t layout_renders_ = 0;
};
void PrepareText(const graph::ExecutionPlan& plan, std::span<const project::PackagedAsset> assets,
                 assets::Images& images, std::size_t model_image_bytes, TextCache& cache,
                 std::stop_token stop, const assets::Images& previous);
}  // namespace rhythm::prepared_assets::detail
