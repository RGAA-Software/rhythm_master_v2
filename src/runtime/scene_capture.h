#pragma once
#include "scene_pass.h"
namespace rhythm::runtime::detail {
// One draw pass owns both attachments. Extractors only return value observers;
// changing extent/precision replaces the pair before publishing the next result.
class SceneCapture final {
   public:
    SceneImage Draw(const scene::Scene& scene, const scene::Camera& camera, render::Extent extent,
                    render::TexturePrecision precision, render::Renderer& renderer);

   private:
    ScenePass pass_{};
    render::Texture color_{};
    render::Texture depth_{};
    render::Extent extent_{};
    render::TexturePrecision precision_ = render::TexturePrecision::kFloat16;
};
}  // namespace rhythm::runtime::detail
