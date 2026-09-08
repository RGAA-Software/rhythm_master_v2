#pragma once

#include "rhythm/render/renderer.h"

namespace rhythm::runtime::detail {
// Owns the bounded downsample pyramid and horizontal intermediate. Output stays
// at the requested extent; all resources live and are released on the render thread.
class BlurPass final {
   public:
    render::TextureHandle Draw(
            render::TextureHandle source, render::Extent extent, float radius,
            render::Renderer& renderer,
            render::TexturePrecision precision = render::TexturePrecision::kUnorm8);

   private:
    std::vector<render::Texture> pyramid_{};
    render::Texture horizontal_{};
    render::Texture output_{};
    render::Extent extent_{};
    std::size_t levels_ = 0;
    render::TexturePrecision precision_ = render::TexturePrecision::kUnorm8;
};
}  // namespace rhythm::runtime::detail
