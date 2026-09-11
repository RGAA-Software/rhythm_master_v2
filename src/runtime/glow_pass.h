#pragma once

#include "rhythm/render/renderer.h"

namespace rhythm::runtime::detail {
class GlowPass final {
   public:
    struct Settings {
        float strength_ = 0.8f;
        float threshold_ = 1;
        float threshold_scale_ = 1;
        float bloom_floor_ = 0;
        float luminance_cap_ = 16;
        std::size_t levels_ = 4;
    };
    render::TextureHandle Draw(render::TextureHandle source, render::Extent extent,
                               const Settings& settings, render::Renderer& renderer);

   private:
    std::vector<render::Texture> downsample_{};
    std::vector<render::Texture> upsample_{};
    render::Texture output_{};
    render::Extent extent_{};
    std::size_t levels_ = 0;
};
}  // namespace rhythm::runtime::detail
