#pragma once

#include <span>

#include "bgfx_handles.h"
#include "rhythm/render/scene.h"

namespace rhythm::render::detail {
// Owns optional skinning programs and palette uniforms; vertex streams remain
// with their immutable mesh owner. Called only on the render host thread.
class BgfxSceneSkin final {
   public:
    BgfxSceneSkin();
    GpuHandle<bgfx::VertexBufferHandle> Create(std::span<const SkinWeights> weights) const;
    void Bind(std::span<const Matrix4> bones) const;
    bgfx::ProgramHandle Program(bool instanced) const {
        return instanced ? instances_.Get() : ordinary_.Get();
    }

   private:
    bgfx::VertexLayout layout_{};
    GpuHandle<bgfx::ProgramHandle> ordinary_{};
    GpuHandle<bgfx::ProgramHandle> instances_{};
    GpuHandle<bgfx::UniformHandle> bones_{};
};
}  // namespace rhythm::render::detail
