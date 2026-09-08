#pragma once
#include "bgfx_handles.h"
#include "rhythm/render/scene.h"
namespace rhythm::render::detail {
// Owns private light uniforms and the packed snapshot for the current scene pass.
class BgfxSceneLights final {
   public:
    BgfxSceneLights();
    void Set(const SceneDrawList& list);
    void Bind() const;

   private:
    using Values = std::array<std::array<float, 4>, 4>;
    Values directions_{};
    Values colors_{};
    Values positions_{};
    Values spot_directions_{};
    Values ranges_{};
    std::array<GpuHandle<bgfx::UniformHandle>, 5> uniforms_{};
};
}  // namespace rhythm::render::detail
