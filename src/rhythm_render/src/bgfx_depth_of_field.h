#pragma once

#include <bgfx/bgfx.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "bgfx_handles.h"
#include "rhythm/render/renderer.h"

namespace rhythm::render::detail {
// Render-thread owner of Godot's signed-weight bokeh passes and scratch targets.
class BgfxDepthOfField final {
   public:
    BgfxDepthOfField();
    ~BgfxDepthOfField();
    std::uint32_t PreparationPasses(const DepthOfField& dof) const;
    std::uint64_t AdditionalTextureBytes(Extent extent) const;
    void Prepare(bgfx::ViewId first_view, const DrawCommand& command, Extent extent,
                 float logical_width, float logical_height, bool invert, bool homogeneous_depth,
                 const bgfx::TransientVertexBuffer& vertices,
                 const bgfx::TransientIndexBuffer& indices, bgfx::TextureHandle source,
                 bgfx::TextureHandle depth);
    void SubmitFinal(bgfx::ViewId view, const DrawCommand& command, Extent extent,
                     bgfx::TextureHandle source);
    // Retires cached scratch targets that no depth of field command used during
    // the frame. Called at frame end so a released scene cannot hold an idle
    // high-water pool; a later depth of field draw recreates its extent's set.
    void ReleaseUnused();
    void AddStats(FrameStats& stats) const;

   private:
    struct Buffers;
    Buffers& GetBuffers(Extent extent);
    void SetFilterUniforms(const DepthOfField& dof, Extent sampling_extent, bool second_pass) const;
    void DrawPass(bgfx::ViewId view, const char* name, bgfx::FrameBufferHandle framebuffer,
                  Extent extent, float logical_width, float logical_height, bool invert,
                  bool homogeneous_depth, const DrawCommand& command,
                  const bgfx::TransientVertexBuffer& vertices,
                  const bgfx::TransientIndexBuffer& indices, bgfx::ProgramHandle program) const;

    GpuHandle<bgfx::ProgramHandle> weight_program_{};
    GpuHandle<bgfx::ProgramHandle> filter_program_{};
    GpuHandle<bgfx::ProgramHandle> final_filter_program_{};
    GpuHandle<bgfx::ProgramHandle> composite_program_{};
    GpuHandle<bgfx::UniformHandle> sampler_{};
    GpuHandle<bgfx::UniformHandle> weight_sampler_{};
    GpuHandle<bgfx::UniformHandle> original_weight_sampler_{};
    GpuHandle<bgfx::UniformHandle> original_color_sampler_{};
    GpuHandle<bgfx::UniformHandle> depth_settings_{};
    GpuHandle<bgfx::UniformHandle> dof_settings_{};
    GpuHandle<bgfx::UniformHandle> filter_settings_{};
    GpuHandle<bgfx::UniformHandle> domain_{};
    std::vector<std::unique_ptr<Buffers>> buffers_{};
};
}  // namespace rhythm::render::detail
