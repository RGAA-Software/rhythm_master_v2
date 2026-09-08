#include "scene_capture.h"

#include <stdexcept>
namespace rhythm::runtime::detail {
SceneImage SceneCapture::Draw(const scene::Scene& scene, const scene::Camera& camera,
                              render::Extent extent, render::TexturePrecision precision,
                              render::Renderer& renderer) {
    if (!renderer.SupportsSampleableDepth())
        throw std::runtime_error("render.sampleable_depth_unsupported");
    const auto list = pass_.Build(scene, camera, extent, renderer);
    if (extent_ != extent || precision_ != precision || !renderer.IsValid(color_.Handle()) ||
        !renderer.IsValid(depth_.Handle())) {
        color_ = {};
        depth_ = {};
        color_ = renderer.CreateTexture(extent, {}, precision);
        depth_ = renderer.CreateDepthTexture(extent);
        extent_ = extent;
        precision_ = precision;
    }
    renderer.SubmitSceneDepth(color_.Handle(), depth_.Handle(), list);
    return {color_.Handle(),
            {depth_.Handle(),
             {float(camera.near_), float(camera.far_),
              camera.kind_ == scene::ProjectionKind::kOrthographic}}};
}
}  // namespace rhythm::runtime::detail
