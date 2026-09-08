#include "bgfx_image_programs.h"

#include <bx/platform.h>

#include <stdexcept>

#include "rhythm/image_shader/program.h"
#include "rhythm/render/budget.h"
#include "vs_ocornut_imgui.bin.h"

namespace rhythm::render::detail {
BgfxImagePrograms::BgfxImagePrograms(std::uint64_t device) : store_(device) {
#if BX_PLATFORM_ANDROID
    vertex_ = GpuHandle(
            bgfx::createShader(bgfx::copy(vs_ocornut_imgui_essl, sizeof(vs_ocornut_imgui_essl))));
#else
    vertex_ = GpuHandle(
            bgfx::createShader(bgfx::copy(vs_ocornut_imgui_dxbc, sizeof(vs_ocornut_imgui_dxbc))));
#endif
    sampler_ = GpuHandle(bgfx::createUniform("s_tex", bgfx::UniformType::Sampler));
    parameters_ = GpuHandle(bgfx::createUniform("u_image_params", bgfx::UniformType::Vec4));
    information_ = GpuHandle(bgfx::createUniform("u_image_info", bgfx::UniformType::Vec4));
    if (!bgfx::isValid(vertex_.Get()) || !bgfx::isValid(sampler_.Get()) ||
        !bgfx::isValid(parameters_.Get()) || !bgfx::isValid(information_.Get()))
        throw BudgetExceeded(Budget::kBackendResources);
}
ImageProgramHandle BgfxImagePrograms::Create(std::span<const std::uint8_t> artifact) {
#if BX_PLATFORM_ANDROID
    image_shader::ValidateArtifact(artifact, image_shader::Target::kGles300);
#else
    image_shader::ValidateArtifact(artifact, image_shader::Target::kWindowsSm5);
#endif
    const auto handle = store_.Allocate(artifact);
    try {
        GpuHandle fragment(
                bgfx::createShader(bgfx::copy(artifact.data(), std::uint32_t(artifact.size()))));
        if (!bgfx::isValid(fragment.Get()))
            throw std::runtime_error("render.image_program_compile");
        GpuHandle program(bgfx::createProgram(vertex_.Get(), fragment.Get(), false));
        if (!bgfx::isValid(program.Get())) throw std::runtime_error("render.image_program_link");
        if (programs_.size() <= handle.slot_) programs_.resize(handle.slot_ + 1);
        programs_[handle.slot_] = std::move(program);
    } catch (...) {
        store_.Release(handle);
        throw;
    }
    return handle;
}
void BgfxImagePrograms::Release(ImageProgramHandle handle) noexcept {
    if (!store_.Owns(handle)) return;
    programs_[handle.slot_] = {};
    store_.Release(handle);
}
void BgfxImagePrograms::Submit(bgfx::ViewId view, const ImageProgramInput& input,
                               bgfx::TextureHandle source, Extent extent) const {
    const std::array information{input.seconds_, float(extent.width_), float(extent.height_), 0.0f};
    bgfx::setTexture(0, sampler_.Get(), source);
    bgfx::setUniform(parameters_.Get(), input.parameters_.data());
    bgfx::setUniform(information_.Get(), information.data());
    bgfx::submit(view, programs_.at(input.program_.slot_).Get());
}
}  // namespace rhythm::render::detail
