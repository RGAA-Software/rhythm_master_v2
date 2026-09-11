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
    struct DisplaySettings {
        render::GlowBlendMode mode_ = render::GlowBlendMode::kAdd;
        render::ColorPipeline pipeline_{render::ColorTransfer::kLinear,
                                        render::ColorTransfer::kSrgb,
                                        render::ToneMapping::kReinhard};
    };
    render::TextureHandle Draw(render::TextureHandle source, render::Extent extent,
                               const Settings& settings, render::Renderer& renderer);
    render::TextureHandle DrawDisplay(render::TextureHandle source, render::Extent extent,
                                      const Settings& settings, const DisplaySettings& display,
                                      render::Renderer& renderer);

   private:
    render::TextureHandle BuildGlow(render::TextureHandle source, render::Extent extent,
                                    const Settings& settings, render::TexturePrecision output,
                                    render::Renderer& renderer);
    std::vector<render::Texture> downsample_{};
    std::vector<render::Texture> upsample_{};
    render::Texture output_{};
    render::Extent extent_{};
    std::size_t levels_ = 0;
    render::TexturePrecision output_precision_ = render::TexturePrecision::kFloat16;
};
}  // namespace rhythm::runtime::detail
