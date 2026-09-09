#pragma once

#include "scene_pass.h"

namespace rhythm::runtime::detail {
// Owns color output and the optional high-resolution scene attachment. Resolving
// color never changes the scene.capture color/depth pair or caller canvas size.
class SceneColor final {
   public:
    render::TextureHandle Draw(const scene::Scene& scene, const scene::Camera& camera,
                               render::Extent extent, render::TexturePrecision precision,
                               bool supersample, render::Renderer& renderer,
                               std::span<const NodeOutput> outputs);

   private:
    ScenePass pass_{};
    render::Texture color_{};
    render::Texture high_{};
    render::Extent extent_{};
    render::TexturePrecision precision_ = render::TexturePrecision::kUnorm8;
    bool supersample_ = false;
};
}  // namespace rhythm::runtime::detail
