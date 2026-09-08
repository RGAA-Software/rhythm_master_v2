#pragma once

#include <span>

#include "bgfx_handles.h"
#include "rhythm/render/scene.h"

namespace rhythm::render::detail {
struct MorphGeometry {
    GpuHandle<bgfx::VertexBufferHandle> lookup_{};
    GpuHandle<bgfx::TextureHandle> deltas_{};
    std::uint32_t vertices_ = 0;
    std::uint16_t height_ = 0;
    std::uint8_t targets_ = 0;
};
// Optional immutable target-texture uploads and vertex animation programs.
// All methods and owned native resources stay on the render host thread.
class BgfxSceneMorph final {
   public:
    BgfxSceneMorph();
    MorphGeometry Create(std::size_t vertices, std::span<const SkinWeights> skin,
                         std::span<const MorphTarget> targets) const;
    void Bind(const MorphGeometry& geometry, const MeshDraw& draw) const;
    bgfx::ProgramHandle Program(bool instanced) const {
        return instanced ? instances_.Get() : ordinary_.Get();
    }

   private:
    bgfx::VertexLayout layout_{};
    GpuHandle<bgfx::ProgramHandle> ordinary_{};
    GpuHandle<bgfx::ProgramHandle> instances_{};
    GpuHandle<bgfx::UniformHandle> bones_{};
    GpuHandle<bgfx::UniformHandle> weights_{};
    GpuHandle<bgfx::UniformHandle> info_{};
    GpuHandle<bgfx::UniformHandle> sampler_{};
};
}  // namespace rhythm::render::detail
