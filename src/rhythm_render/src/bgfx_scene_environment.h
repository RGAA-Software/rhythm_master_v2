#pragma once

#include "bgfx_scene_textures.h"

namespace rhythm::render::detail {
// Render-thread receiver bindings; the runtime owns the filtered environment.
class BgfxSceneEnvironment final {
   public:
    BgfxSceneEnvironment();
    void Bind(const std::optional<SceneEnvironment>& environment,
              const SceneTextureResolver& resolve) const;

   private:
    GpuHandle<bgfx::UniformHandle> sampler_{};
    GpuHandle<bgfx::UniformHandle> settings_{};
    GpuHandle<bgfx::TextureHandle> fallback_{};
};
}  // namespace rhythm::render::detail
