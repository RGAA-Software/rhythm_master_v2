#include "trail_pass.h"

#include <cmath>
#include <stdexcept>
#include <utility>

#include "texture_ops.h"

namespace rhythm::runtime::detail {
render::TextureHandle TrailPass::Draw(render::TextureHandle source, render::Extent extent,
                                      double seconds, bool advance, TrailSettings settings,
                                      render::Renderer& renderer) {
    if (!std::isfinite(seconds) || seconds < 0 || !std::isfinite(settings.half_life_) ||
        settings.half_life_ < 0 || settings.half_life_ > 5 || !std::isfinite(settings.zoom_rate_) ||
        std::abs(settings.zoom_rate_) > 0.5 || !std::isfinite(settings.rotation_rate_) ||
        std::abs(settings.rotation_rate_) > 180 || !renderer.IsValid(source) || !extent.width_ ||
        !extent.height_)
        throw std::invalid_argument("runtime.trail_settings");
    if (settings.half_life_ == 0) {
        *this = {};
        return source;
    }
    if (extent_ != extent) {
        *this = {};
        auto history = renderer.CreateTexture(extent, {}, render::TexturePrecision::kFloat16);
        auto target = renderer.CreateTexture(extent, {}, render::TexturePrecision::kFloat16);
        history_ = std::move(history);
        target_ = std::move(target);
        extent_ = extent;
    }
    const auto elapsed = last_seconds_ ? seconds - *last_seconds_ : 0;
    if (last_seconds_ && (!advance || elapsed == 0)) {
        last_seconds_ = seconds;
        return history_.Handle();
    }
    render::DrawList draw;
    draw.width_ = extent.width_;
    draw.height_ = extent.height_;
    AppendTextureQuad(draw, source, 0xffffffff, 0xffffffff);
    if (last_seconds_ && elapsed > 0 && elapsed <= 0.25) {
        draw.commands_.back().texture_trail_ = render::TextureTrail{
                history_.Handle(), static_cast<float>(std::exp2(-elapsed / settings.half_life_)),
                static_cast<float>(std::exp(settings.zoom_rate_ * elapsed)),
                static_cast<float>(settings.rotation_rate_ * elapsed)};
    }
    renderer.Submit(target_.Handle(), draw);
    std::swap(history_, target_);
    last_seconds_ = seconds;
    return history_.Handle();
}
}  // namespace rhythm::runtime::detail
