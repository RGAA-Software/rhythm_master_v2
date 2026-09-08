#pragma once

#include "bgfx_handles.h"
#include "rhythm/render/renderer.h"

namespace rhythm::render::detail {
// Render-thread shader resources and uniform binding. Must die before the device.
class BgfxTexturePrograms final {
   public:
    BgfxTexturePrograms();
    void Submit(std::uint16_t view, const DrawCommand& command, Extent source_size, float aspect,
                bgfx::TextureHandle source, Extent map_size, bgfx::TextureHandle map,
                bool float_target) const;

   private:
    GpuHandle<bgfx::ProgramHandle> environment_program_{};
    GpuHandle<bgfx::UniformHandle> environment_settings_{};
    GpuHandle<bgfx::ProgramHandle> depth_program_{};
    GpuHandle<bgfx::UniformHandle> depth_settings_{};
    GpuHandle<bgfx::ProgramHandle> dof_program_{};
    GpuHandle<bgfx::UniformHandle> dof_settings_{};
    GpuHandle<bgfx::UniformHandle> dof_domain_{};
    GpuHandle<bgfx::ProgramHandle> pipeline_program_{};
    GpuHandle<bgfx::UniformHandle> pipeline_settings_{};
    GpuHandle<bgfx::UniformHandle> color_limits_{};
    GpuHandle<bgfx::ProgramHandle> trail_program_{};
    GpuHandle<bgfx::UniformHandle> trail_settings_{};
    GpuHandle<bgfx::ProgramHandle> displace_program_{};
    GpuHandle<bgfx::UniformHandle> displace_settings_{};
    GpuHandle<bgfx::UniformHandle> displace_domain_{};
    GpuHandle<bgfx::UniformHandle> map_sampler_{};
    GpuHandle<bgfx::ProgramHandle> mapping_program_{};
    GpuHandle<bgfx::UniformHandle> mapping_settings_{};
    GpuHandle<bgfx::UniformHandle> mapping_domain_{};
    GpuHandle<bgfx::ProgramHandle> contours_program_{};
    GpuHandle<bgfx::UniformHandle> contours_settings_{};
    GpuHandle<bgfx::UniformHandle> contours_color_a_{};
    GpuHandle<bgfx::UniformHandle> contours_color_b_{};
    GpuHandle<bgfx::ProgramHandle> program_{};
    GpuHandle<bgfx::ProgramHandle> color_program_{};
    GpuHandle<bgfx::UniformHandle> color_uniform_{};
    GpuHandle<bgfx::UniformHandle> sampler_{};
    GpuHandle<bgfx::ProgramHandle> filter_program_{};
    GpuHandle<bgfx::UniformHandle> filter_uniform_{};
    GpuHandle<bgfx::ProgramHandle> noise_program_{};
    GpuHandle<bgfx::UniformHandle> noise_settings_{};
    GpuHandle<bgfx::UniformHandle> noise_color_a_{};
    GpuHandle<bgfx::UniformHandle> noise_color_b_{};
    GpuHandle<bgfx::UniformHandle> noise_domain_{};
};
}  // namespace rhythm::render::detail
