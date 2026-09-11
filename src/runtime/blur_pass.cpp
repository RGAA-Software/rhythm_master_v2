#include "blur_pass.h"

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
void DrawTexture(render::Renderer& renderer, render::TextureHandle source,
                 render::TextureHandle target, render::Extent extent,
                 std::optional<render::TextureFilter> filter = std::nullopt) {
    render::DrawList list;
    list.width_ = extent.width_;
    list.height_ = extent.height_;
    AppendTextureQuad(list, source, 0xffffffff, 0xffffffff);
    list.commands_.back().texture_filter_ = filter;
    renderer.Submit(target, list);
}
}  // namespace
render::TextureHandle BlurPass::Draw(render::TextureHandle source, render::Extent extent,
                                     float radius, render::Renderer& renderer,
                                     render::TexturePrecision precision) {
    if (!std::isfinite(radius) || radius < 0 || radius > 256 || !extent.width_ || !extent.height_)
        throw std::invalid_argument("runtime.blur_parameters");
    if (radius == 0) {
        pyramid_.clear();
        if (renderer.Precision(source) == precision) {
            output_ = {};
            return source;
        }
        if (extent != extent_ || precision != precision_ || !renderer.IsValid(output_.Handle())) {
            output_ = {};
            output_ = renderer.CreateTexture(extent, {}, precision);
        }
        extent_ = extent;
        precision_ = precision;
        levels_ = 0;
        DrawTexture(renderer, source, output_.Handle(), extent,
                    render::TextureFilter{render::TextureFilterKind::kGodotGaussian, 0, 0});
        return output_.Handle();
    }
    auto reduced = extent;
    auto step = radius;
    std::size_t levels = 0;
    while (step >= 2 && levels < 6 && reduced.width_ > 1 && reduced.height_ > 1) {
        reduced = Half(reduced);
        step *= 0.5f;
        ++levels;
    }
    if (extent != extent_ || precision != precision_ || levels != levels_ ||
        !renderer.IsValid(output_.Handle())) {
        // Replace intermediate sizes together; no source texture is owned here.
        pyramid_.clear();
        output_ = {};
        auto size = extent;
        for (std::size_t index = 0; index < levels; ++index) {
            size = Half(size);
            pyramid_.push_back(renderer.CreateTexture(size, {}, precision));
        }
        output_ = renderer.CreateTexture(extent, {}, precision);
        extent_ = extent;
        levels_ = levels;
        precision_ = precision;
    }
    auto size = extent;
    for (std::size_t index = 0; index < pyramid_.size(); ++index) {
        const auto source_size = size;
        size = Half(source_size);
        const auto scale = index + 1 == pyramid_.size() ? step : 1.0f;
        const render::TextureFilter filter{render::TextureFilterKind::kGodotGaussian,
                                           scale * source_size.width_ / size.width_,
                                           scale * source_size.height_ / size.height_};
        auto& target = pyramid_[index];
        DrawTexture(renderer, source, target.Handle(), size, filter);
        source = target.Handle();
    }
    if (levels == 0) {
        DrawTexture(renderer, source, output_.Handle(), extent,
                    render::TextureFilter{render::TextureFilterKind::kGodotGaussian, step, step});
    } else {
        DrawTexture(renderer, source, output_.Handle(), extent);
    }
    return output_.Handle();
}
}  // namespace rhythm::runtime::detail
