#include "scene_color.h"

#include <stdexcept>

namespace rhythm::runtime::detail {
render::TextureHandle SceneColor::Draw(const scene::Scene& scene, const scene::Camera& camera,
                                       render::Extent extent, render::TexturePrecision precision,
                                       bool supersample, render::Renderer& renderer,
                                       std::span<const NodeOutput> outputs) {
    if (supersample && (extent.width_ > 4096 || extent.height_ > 4096))
        throw std::invalid_argument("runtime.scene_sampling_extent");
    if (extent_ != extent || precision_ != precision || supersample_ != supersample ||
        !renderer.IsValid(color_.Handle())) {
        high_ = {};
        color_ = {};
        color_ = renderer.CreateTexture(extent, {}, precision);
        extent_ = extent;
        precision_ = precision;
        supersample_ = supersample;
    }
    if (!supersample) high_ = {};
    const auto list = pass_.Build(scene, camera, extent, renderer, outputs);
    if (!supersample) {
        renderer.SubmitScene(color_.Handle(), list);
        return color_.Handle();
    }
    if (!renderer.IsValid(high_.Handle()))
        high_ = renderer.CreateTexture(
                {std::uint16_t(extent.width_ * 2), std::uint16_t(extent.height_ * 2)}, {},
                precision);
    renderer.SubmitScene(high_.Handle(), list);
    render::DrawList resolve;
    const auto width = float(extent.width_), height = float(extent.height_);
    resolve.width_ = width;
    resolve.height_ = height;
    resolve.vertices_ = {{0, 0, 0, 0}, {width, 0, 1, 0}, {width, height, 1, 1}, {0, height, 0, 1}};
    resolve.indices_ = {0, 1, 2, 0, 2, 3};
    resolve.commands_ = {{high_.Handle(), 0, 6, {0, 0, width, height}}};
    renderer.Submit(color_.Handle(), resolve);
    return color_.Handle();
}
}  // namespace rhythm::runtime::detail
