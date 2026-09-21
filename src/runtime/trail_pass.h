#pragma once

#include "rhythm/render/renderer.h"

namespace rhythm::runtime::detail {
struct TrailSettings {
    double half_life_ = 0.5;
    double zoom_rate_ = 0;
    double rotation_rate_ = 0;
};
// Render-thread temporal state. Holds exactly two float targets. Repeated, paused
// or reversed times preserve pixels; stalls longer than 250 ms decay by a bounded
// step so hitches converge the trail instead of cutting it.
class TrailPass final {
   public:
    render::TextureHandle Draw(render::TextureHandle source, render::Extent extent, double seconds,
                               bool advance, TrailSettings settings, render::Renderer& renderer);

   private:
    render::Texture history_{};
    render::Texture target_{};
    render::Extent extent_{};
    std::optional<double> last_seconds_{};
};
}  // namespace rhythm::runtime::detail
