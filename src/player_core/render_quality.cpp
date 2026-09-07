#include "rhythm/player/render_quality.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace rhythm::player {
render::Extent PlaybackExtent(render::Extent canvas, RenderQuality quality) {
    if (!canvas.width_ || !canvas.height_) throw std::invalid_argument("player.empty_canvas");
    if (quality == RenderQuality::kOriginal) return canvas;
    if (quality != RenderQuality::kBalanced && quality != RenderQuality::kEconomy)
        throw std::invalid_argument("player.render_quality");
    const double edge = quality == RenderQuality::kBalanced ? 960 : 640;
    const double pixels = quality == RenderQuality::kBalanced ? 960 * 540 : 640 * 360;
    const auto scale = std::min({1.0, edge / std::max(canvas.width_, canvas.height_),
                                 std::sqrt(pixels / (double(canvas.width_) * canvas.height_))});
    return {static_cast<std::uint16_t>(std::max(1.0, std::floor(canvas.width_ * scale))),
            static_cast<std::uint16_t>(std::max(1.0, std::floor(canvas.height_ * scale)))};
}
}  // namespace rhythm::player
