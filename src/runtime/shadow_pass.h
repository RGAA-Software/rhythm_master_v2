#pragma once

#include "rhythm/render/renderer.h"
#include "rhythm/scene/camera.h"
#include "rhythm/scene/scene.h"

namespace rhythm::runtime::detail {
// Builds the selected light camera. Directional projections snap their X/Y
// center to the shadow-map texel grid; spot projections remain light-relative.
scene::Camera ShadowCamera(const scene::Scene& scene);

// Host-thread owner of one bounded shadow depth/color pair. Reuses the ordinary
// scene depth pass; only opaque geometry casts. ScenePass calls Apply after
// resolving mesh/material instances and before submitting the receiver scene.
class ShadowPass final {
   public:
    void Apply(const scene::Scene& scene, render::SceneDrawList& receivers,
               render::Renderer& renderer);

   private:
    render::Texture color_{};
    render::Texture depth_{};
    std::uint16_t resolution_ = 0;
};
}  // namespace rhythm::runtime::detail
