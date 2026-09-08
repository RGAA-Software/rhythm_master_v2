#pragma once

#include "bgfx_scene_textures.h"

namespace rhythm::render::detail {
// Owns receiver bindings only. Runtime owns the shadow depth image and camera;
// the backend borrows that image for this synchronous submission.
class BgfxSceneShadow final {
   public:
    BgfxSceneShadow();
    void Bind(const std::optional<SceneShadow>& shadow, const SceneTextureResolver& resolve) const;

   private:
    GpuHandle<bgfx::UniformHandle> sampler_{};
    GpuHandle<bgfx::UniformHandle> matrix_{};
    GpuHandle<bgfx::UniformHandle> settings_{};
    GpuHandle<bgfx::UniformHandle> filtering_{};
    GpuHandle<bgfx::TextureHandle> white_{};
};
}  // namespace rhythm::render::detail
