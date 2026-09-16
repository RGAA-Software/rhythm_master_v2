#pragma once

#include <array>

#include "rhythm/render/renderer.h"
#include "rhythm/scene/camera.h"
#include "rhythm/scene/scene.h"

namespace rhythm::runtime::detail {
// Builds the selected light camera. Directional projections snap their X/Y
// center to the shadow-map texel grid; spot projections remain light-relative.
scene::Camera ShadowCamera(const scene::Scene& scene);
struct DirectionalCascadeCameras {
    std::array<scene::Camera, 2> cameras_{};
    double split_depth_ = 0;
};
// Fits Godot-style stable orthographic projections around two receiver-camera
// frustum segments. The selected light must be directional.
DirectionalCascadeCameras CascadeCameras(const scene::Scene& scene,
                                         const scene::Camera& receiver_camera, double aspect);

// Host-thread owner of one bounded shadow depth/color pair. Reuses the ordinary
// scene depth pass; only opaque geometry casts. ScenePass calls Apply after
// resolving mesh/material instances and before submitting the receiver scene.
class ShadowPass final {
   public:
    void Apply(const scene::Scene& scene, const scene::Camera& receiver_camera, double aspect,
               render::SceneDrawList& receivers, render::Renderer& renderer);

   private:
    std::array<render::Texture, 2> colors_{};
    std::array<render::Texture, 2> depths_{};
    std::uint16_t resolution_ = 0;
    std::uint8_t cascades_ = 0;
};
}  // namespace rhythm::runtime::detail
