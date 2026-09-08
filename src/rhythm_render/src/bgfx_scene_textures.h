#pragma once

#include <functional>

#include "bgfx_handles.h"
#include "rhythm/render/scene.h"

namespace rhythm::render::detail {
using SceneTextureResolver = std::function<bgfx::TextureHandle(TextureHandle)>;
// Owns sampler uniforms and neutral fallback texel. Resolution borrows backend
// textures synchronously; no native texture observer is retained between draws.
class BgfxSceneTextures final {
   public:
    BgfxSceneTextures();
    void Bind(const MaterialTextures& textures, const SceneTextureResolver& resolve) const;

   private:
    std::array<GpuHandle<bgfx::UniformHandle>, 4> samplers_{};
    GpuHandle<bgfx::UniformHandle> enabled_{};
    GpuHandle<bgfx::UniformHandle> options_{};
    GpuHandle<bgfx::UniformHandle> uv_{};
    GpuHandle<bgfx::TextureHandle> white_{};
};
}  // namespace rhythm::render::detail
