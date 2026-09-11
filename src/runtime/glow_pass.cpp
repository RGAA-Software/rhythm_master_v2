#include "glow_pass.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "texture_ops.h"

namespace rhythm::runtime::detail {
namespace {
render::Extent Half(render::Extent size) {
    return {static_cast<std::uint16_t>(std::max(1, (size.width_ + 1) / 2)),
            static_cast<std::uint16_t>(std::max(1, (size.height_ + 1) / 2))};
}
void DrawStage(render::Renderer& renderer, render::TextureHandle source,
               render::TextureHandle target, render::Extent target_size,
               const render::TextureGlow& glow) {
    render::DrawList list;
    list.width_ = target_size.width_;
    list.height_ = target_size.height_;
    AppendTextureQuad(list, source, 0xffffffff, 0xffffffff);
    list.commands_.back().texture_glow_ = glow;
    renderer.Submit(target, list);
}
}  // namespace
render::TextureHandle GlowPass::BuildGlow(render::TextureHandle source, render::Extent extent,
                                          const Settings& settings,
                                          render::TexturePrecision output_precision,
                                          render::Renderer& renderer) {
    const auto finite = [](float value) { return std::isfinite(value); };
    if (!extent.width_ || !extent.height_ || !finite(settings.strength_) ||
        !finite(settings.threshold_) || !finite(settings.threshold_scale_) ||
        !finite(settings.bloom_floor_) || !finite(settings.luminance_cap_) ||
        settings.strength_ < 0 || settings.strength_ > 4 || settings.threshold_ < 0 ||
        settings.threshold_ > 16 || settings.threshold_scale_ < 0.001f ||
        settings.threshold_scale_ > 16 || settings.bloom_floor_ < 0 || settings.bloom_floor_ > 1 ||
        settings.luminance_cap_ < 0.01f || settings.luminance_cap_ > 65504 ||
        settings.levels_ < 1 || settings.levels_ > 6)
        throw std::invalid_argument("runtime.glow_parameters");
    auto size = extent;
    std::vector<render::Extent> sizes;
    std::size_t levels = 0;
    while (levels < settings.levels_ && size.width_ > 1 && size.height_ > 1) {
        size = Half(size);
        sizes.push_back(size);
        ++levels;
    }
    if (extent != extent_ || levels != levels_ || output_precision != output_precision_ ||
        !renderer.IsValid(output_.Handle())) {
        downsample_.clear();
        upsample_.clear();
        output_ = {};
        size = extent;
        for (std::size_t index = 0; index < levels; ++index) {
            size = Half(size);
            downsample_.push_back(
                    renderer.CreateTexture(size, {}, render::TexturePrecision::kFloat16));
        }
        for (std::size_t index = 0; index + 1 < levels; ++index) {
            upsample_.push_back(
                    renderer.CreateTexture(sizes[index], {}, render::TexturePrecision::kFloat16));
        }
        output_ = renderer.CreateTexture(extent, {}, output_precision);
        extent_ = extent;
        levels_ = levels;
        output_precision_ = output_precision;
    }
    if (!levels) return {};
    render::TextureGlow filter;
    filter.kind_ = render::TextureGlowKind::kFilter;
    filter.threshold_ = settings.threshold_;
    filter.threshold_scale_ = settings.threshold_scale_;
    filter.bloom_floor_ = settings.bloom_floor_;
    filter.luminance_cap_ = settings.luminance_cap_;
    DrawStage(renderer, source, downsample_[0].Handle(), sizes[0], filter);
    render::TextureGlow down;
    down.kind_ = render::TextureGlowKind::kDownsample;
    for (std::size_t index = 1; index < levels; ++index)
        DrawStage(renderer, downsample_[index - 1].Handle(), downsample_[index].Handle(),
                  sizes[index], down);
    auto glow = downsample_.back().Handle();
    render::TextureGlow up;
    up.kind_ = render::TextureGlowKind::kUpsample;
    for (std::size_t index = levels - 1; index-- > 0;) {
        DrawStage(renderer, glow, upsample_[index].Handle(), sizes[index], up);
        glow = upsample_[index].Handle();
    }
    return glow;
}
render::TextureHandle GlowPass::Draw(render::TextureHandle source, render::Extent extent,
                                     const Settings& settings, render::Renderer& renderer) {
    const auto glow =
            BuildGlow(source, extent, settings, render::TexturePrecision::kFloat16, renderer);
    if (!glow.device_) return source;
    render::DrawList composite;
    composite.width_ = extent.width_;
    composite.height_ = extent.height_;
    AppendTextureQuad(composite, source, 0xffffffff, 0xffffffff);
    AppendTextureQuad(composite, glow, 0xffffffff, 0xffffffff);
    composite.commands_.back().blend_ = render::BlendMode::kAdd;
    render::TextureGlow addition;
    addition.kind_ = render::TextureGlowKind::kComposite;
    addition.strength_ = settings.strength_;
    composite.commands_.back().texture_glow_ = addition;
    renderer.Submit(output_.Handle(), composite);
    return output_.Handle();
}
render::TextureHandle GlowPass::DrawDisplay(render::TextureHandle source, render::Extent extent,
                                            const Settings& settings,
                                            const DisplaySettings& display,
                                            render::Renderer& renderer) {
    const auto glow =
            BuildGlow(source, extent, settings, render::TexturePrecision::kUnorm8, renderer);
    render::DrawList composite;
    composite.width_ = extent.width_;
    composite.height_ = extent.height_;
    AppendTextureQuad(composite, source, 0xffffffff, 0xffffffff);
    if (glow.device_) {
        composite.commands_.back().texture_glow_display_ = render::TextureGlowDisplay{
                glow, display.mode_, settings.strength_, display.pipeline_};
    } else {
        composite.commands_.back().color_pipeline_ = display.pipeline_;
    }
    renderer.Submit(output_.Handle(), composite);
    return output_.Handle();
}
}  // namespace rhythm::runtime::detail
