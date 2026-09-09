#pragma once

#include "bgfx_handles.h"
#include "surface_program_store.h"

namespace rhythm::render::detail {
// Owns one six-variant scene program group per accepted surface artifact.
// Native resources never leave this render-thread adapter.
class BgfxSurfacePrograms final {
   public:
    explicit BgfxSurfacePrograms(std::uint64_t device);
    SurfaceProgramHandle Create(std::span<const std::uint8_t> artifact);
    void Release(SurfaceProgramHandle handle) noexcept;
    bool IsValid(SurfaceProgramHandle handle) const { return store_.IsValid(handle); }
    void Validate(const SurfaceProgramInput& input) const { store_.Validate(input); }
    bgfx::ProgramHandle Bind(const SurfaceProgramInput& input, bool skin, bool morph,
                             bool instances) const;
    void AddStats(FrameStats& stats) const { store_.AddStats(stats); }
    void Invalidate() { store_.Invalidate(); }

   private:
    SurfaceProgramStore store_{0};
    std::array<GpuHandle<bgfx::ShaderHandle>, 6> vertices_{};
    std::vector<std::array<GpuHandle<bgfx::ProgramHandle>, 6>> programs_{};
    GpuHandle<bgfx::UniformHandle> parameters_{};
    GpuHandle<bgfx::UniformHandle> information_{};
};
}  // namespace rhythm::render::detail
