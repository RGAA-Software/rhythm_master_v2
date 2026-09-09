#include "bgfx_surface_programs.h"

#include <bx/platform.h>

#include "rhythm/shader_artifact/artifact.h"
#include "scene_shader.h"

namespace rhythm::render::detail {
BgfxSurfacePrograms::BgfxSurfacePrograms(std::uint64_t device) : store_(device) {
    const std::array<std::span<const std::uint8_t>, 6> shaders{
            kSceneVertexShader,       kSceneInstanceShader,    kSceneSkinVertexShader,
            kSceneSkinInstanceShader, kSceneMorphVertexShader, kSceneMorphInstanceShader};
    for (std::size_t index = 0; index < shaders.size(); ++index)
        vertices_[index] = GpuHandle(bgfx::createShader(
                bgfx::copy(shaders[index].data(), std::uint32_t(shaders[index].size()))));
    parameters_ = GpuHandle(bgfx::createUniform("u_surface_params", bgfx::UniformType::Vec4));
    information_ = GpuHandle(bgfx::createUniform("u_surface_info", bgfx::UniformType::Vec4));
}
SurfaceProgramHandle BgfxSurfacePrograms::Create(std::span<const std::uint8_t> artifact) {
#if BX_PLATFORM_ANDROID
    constexpr auto kTarget = shader_artifact::Target::kGles300;
#else
    constexpr auto kTarget = shader_artifact::Target::kWindowsSm5;
#endif
    shader_artifact::Validate(artifact, kTarget, shader_artifact::Profile::kSurfaceRgb);
    const auto handle = store_.Allocate(artifact);
    try {
        GpuHandle fragment(
                bgfx::createShader(bgfx::copy(artifact.data(), std::uint32_t(artifact.size()))));
        std::array<GpuHandle<bgfx::ProgramHandle>, 6> candidate;
        for (std::size_t index = 0; index < vertices_.size(); ++index)
            candidate[index] =
                    GpuHandle(bgfx::createProgram(vertices_[index].Get(), fragment.Get(), false));
        if (programs_.size() <= handle.slot_) programs_.resize(handle.slot_ + 1);
        programs_[handle.slot_] = std::move(candidate);
    } catch (...) {
        store_.Release(handle);
        throw;
    }
    return handle;
}
void BgfxSurfacePrograms::Release(SurfaceProgramHandle handle) noexcept {
    if (!store_.Owns(handle)) return;
    programs_[handle.slot_] = {};
    store_.Release(handle);
}
bgfx::ProgramHandle BgfxSurfacePrograms::Bind(const SurfaceProgramInput& input, bool skin,
                                              bool morph, bool instances) const {
    const std::array information{input.seconds_, 0.0f, 0.0f, 0.0f};
    bgfx::setUniform(parameters_.Get(), input.parameters_.data());
    bgfx::setUniform(information_.Get(), information.data());
    const auto index = (morph ? 4 : skin ? 2 : 0) + (instances ? 1 : 0);
    return programs_.at(input.program_.slot_)[index].Get();
}
}  // namespace rhythm::render::detail
