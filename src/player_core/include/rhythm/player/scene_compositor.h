#pragma once

#include "rhythm/render/renderer.h"

namespace rhythm::player {
struct SceneImage {
    render::TextureHandle texture_{};
    render::Extent canvas_{};
};
// Host/render-thread owner of the one additional SDR target used by a dissolve.
// Inputs are already display-referred UNORM8 scene outputs. Both are aspect-fit
// into the destination and mixed in premultiplied RGBA, including transparent
// letterbox regions. The returned observer is valid until reset/resize/destruction.
class SceneCompositor final {
   public:
    render::TextureHandle Blend(render::Renderer& renderer, SceneImage first, SceneImage second,
                                render::Extent extent, double amount);
    // Completed-frame boundary; release before replacing a host device.
    void ReleaseGraphics();

   private:
    render::Texture target_{};
    render::Extent extent_{};
};
}  // namespace rhythm::player
