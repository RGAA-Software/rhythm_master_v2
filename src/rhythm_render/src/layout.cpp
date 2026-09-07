#include "rhythm/render/layout.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace rhythm::render {
ClipRect AspectFit(Extent source, ClipRect bounds) {
    if (!source.width_ || !source.height_ || !std::isfinite(bounds.x_) ||
        !std::isfinite(bounds.y_) || !std::isfinite(bounds.width_) ||
        !std::isfinite(bounds.height_) || bounds.width_ < 0 || bounds.height_ < 0)
        throw std::invalid_argument("render.aspect_bounds");
    const auto scale = std::min(bounds.width_ / source.width_, bounds.height_ / source.height_);
    const auto width = source.width_ * scale;
    const auto height = source.height_ * scale;
    return {bounds.x_ + (bounds.width_ - width) * 0.5f,
            bounds.y_ + (bounds.height_ - height) * 0.5f, width, height};
}
}  // namespace rhythm::render
