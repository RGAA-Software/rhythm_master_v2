#include "bgfx_scene_skin.h"

#include <stdexcept>

#include "scene_shader.h"

namespace rhythm::render::detail {
BgfxSceneSkin::BgfxSceneSkin() {
    static_assert(sizeof(SkinWeights) == 20);
    layout_.begin()
            .add(bgfx::Attrib::Indices, 4, bgfx::AttribType::Uint8, true)
            .add(bgfx::Attrib::Weight, 4, bgfx::AttribType::Float)
            .end();
    GpuHandle vertex(
            bgfx::createShader(bgfx::copy(kSceneSkinVertexShader, sizeof(kSceneSkinVertexShader))));
    GpuHandle instance(bgfx::createShader(
            bgfx::copy(kSceneSkinInstanceShader, sizeof(kSceneSkinInstanceShader))));
    GpuHandle fragment(
            bgfx::createShader(bgfx::copy(kSceneFragmentShader, sizeof(kSceneFragmentShader))));
    ordinary_ = GpuHandle(bgfx::createProgram(vertex.Get(), fragment.Get(), false));
    instances_ = GpuHandle(bgfx::createProgram(instance.Get(), fragment.Get(), false));
    bones_ = GpuHandle(bgfx::createUniform("u_skin_bones", bgfx::UniformType::Mat4,
                                           static_cast<std::uint16_t>(kMaximumSkinBones)));
    if (!bgfx::isValid(ordinary_.Get()) || !bgfx::isValid(instances_.Get()) ||
        !bgfx::isValid(bones_.Get()))
        throw std::runtime_error("render.skin_program");
}
GpuHandle<bgfx::VertexBufferHandle> BgfxSceneSkin::Create(
        std::span<const SkinWeights> weights) const {
    GpuHandle result(bgfx::createVertexBuffer(
            bgfx::copy(weights.data(), static_cast<std::uint32_t>(weights.size_bytes())), layout_));
    if (!bgfx::isValid(result.Get())) throw std::runtime_error("render.skin_buffer");
    return result;
}
void BgfxSceneSkin::Bind(std::span<const Matrix4> bones) const {
    bgfx::setUniform(bones_.Get(), bones.data(), static_cast<std::uint16_t>(bones.size()));
}
}  // namespace rhythm::render::detail
