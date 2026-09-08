#include "rhythm/player/scene_compositor.h"

#include <cmath>
#include <stdexcept>

#include "rhythm/render/layout.h"

namespace rhythm::player {
render::TextureHandle SceneCompositor::Blend(render::Renderer& renderer, SceneImage first,
                                             SceneImage second, render::Extent extent,
                                             double amount) {
    if (!std::isfinite(amount) || amount < 0 || amount > 1 || !extent.width_ || !extent.height_)
        throw std::invalid_argument("player.transition_input");
    for (const auto& image : {first, second})
        if (!renderer.IsValid(image.texture_) || !image.canvas_.width_ || !image.canvas_.height_ ||
            renderer.Precision(image.texture_) != render::TexturePrecision::kUnorm8)
            throw std::invalid_argument("player.transition_image");
    if (extent_ != extent || !renderer.IsValid(target_.Handle())) {
        auto target = renderer.CreateTexture(extent);
        target_ = std::move(target);
        extent_ = extent;
    }
    render::DrawList draw;
    draw.width_ = extent.width_;
    draw.height_ = extent.height_;
    const render::ClipRect bounds{0, 0, draw.width_, draw.height_};
    const auto append = [&](SceneImage image, std::uint32_t alpha) {
        const auto rectangle = render::AspectFit(image.canvas_, bounds);
        const auto left = rectangle.x_, top = rectangle.y_;
        const auto right = left + rectangle.width_, bottom = top + rectangle.height_;
        const auto color = 0x00ffffffU | (alpha << 24);
        const auto base = static_cast<std::uint32_t>(draw.vertices_.size());
        const auto start = static_cast<std::uint32_t>(draw.indices_.size());
        draw.vertices_.insert(draw.vertices_.end(), {{left, top, 0, 0, color},
                                                     {right, top, 1, 0, color},
                                                     {right, bottom, 1, 1, color},
                                                     {left, bottom, 0, 1, color}});
        draw.indices_.insert(draw.indices_.end(),
                             {base, base + 1, base + 2, base, base + 2, base + 3});
        draw.commands_.push_back({image.texture_, start, 6, bounds, render::BlendMode::kAdd});
    };
    // Complementary byte weights preserve unit coverage despite quantization.
    const auto alpha = static_cast<std::uint32_t>(std::lround(amount * 255));
    append(first, 255 - alpha);
    append(second, alpha);
    renderer.Submit(target_.Handle(), draw);
    return target_.Handle();
}
void SceneCompositor::ReleaseGraphics() {
    target_ = {};
    extent_ = {};
}
}  // namespace rhythm::player
