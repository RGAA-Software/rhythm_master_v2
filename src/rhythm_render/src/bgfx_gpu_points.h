#pragma once
#include "bgfx_handles.h"
#include "gpu_point_store.h"
namespace rhythm::render::detail {
// Native-only adapter; buffer ownership and bindings never escape this module.
class BgfxGpuPoints final {
   public:
    explicit BgfxGpuPoints(std::uint64_t device);
    static bool Supported();
    GpuPointHandle Create(std::uint32_t capacity);
    void Release(GpuPointHandle handle) noexcept;
    bool IsValid(GpuPointHandle handle) const { return store_.IsValid(handle); }
    void Update(bgfx::ViewId view, GpuPointHandle handle, const GpuParticleStep& step);
    void ValidateDraw(GpuPointHandle handle, const GpuPointStyle& style) const {
        store_.ValidateDraw(handle, style);
    }
    void Draw(bgfx::ViewId view, bgfx::FrameBufferHandle target, Extent extent, bool invert,
              GpuPointHandle handle, const GpuPointStyle& style, bool float_target,
              bgfx::TextureHandle sampling_texture);
    void AddStats(FrameStats& stats) const { store_.AddStats(stats); }
    void Invalidate() { store_.Invalidate(); }

   private:
    GpuPointStore store_{0};
    std::vector<GpuHandle<bgfx::DynamicVertexBufferHandle>> buffers_{};
    GpuHandle<bgfx::ProgramHandle> compute_{};
    GpuHandle<bgfx::ProgramHandle> render_{};
    GpuHandle<bgfx::VertexBufferHandle> quad_{};
    GpuHandle<bgfx::IndexBufferHandle> indices_{};
    std::array<GpuHandle<bgfx::UniformHandle>, 7> uniforms_{};
    GpuHandle<bgfx::UniformHandle> view_{};
    GpuHandle<bgfx::UniformHandle> sampling_{};
    GpuHandle<bgfx::UniformHandle> sampler_{};
    GpuHandle<bgfx::TextureHandle> white_{};
};
}  // namespace rhythm::render::detail
