#pragma once

#include "bgfx_handles.h"
#include "image_program_store.h"

namespace rhythm::render::detail {
class BgfxImagePrograms final {
   public:
    explicit BgfxImagePrograms(std::uint64_t device);
    ImageProgramHandle Create(std::span<const std::uint8_t> artifact);
    void Release(ImageProgramHandle handle) noexcept;
    bool IsValid(ImageProgramHandle handle) const { return store_.IsValid(handle); }
    void Validate(const ImageProgramInput& input) const { store_.Validate(input); }
    void Submit(bgfx::ViewId view, const ImageProgramInput& input, bgfx::TextureHandle source,
                Extent extent) const;
    void AddStats(FrameStats& stats) const { store_.AddStats(stats); }
    void Invalidate() { store_.Invalidate(); }

   private:
    ImageProgramStore store_{0};
    std::vector<GpuHandle<bgfx::ProgramHandle>> programs_{};
    GpuHandle<bgfx::ShaderHandle> vertex_{};
    GpuHandle<bgfx::UniformHandle> sampler_{};
    GpuHandle<bgfx::UniformHandle> parameters_{};
    GpuHandle<bgfx::UniformHandle> information_{};
};
}  // namespace rhythm::render::detail
