#include "bgfx_scene_instances.h"

#include <cstring>

#include "scene_shader.h"

namespace rhythm::render::detail {
namespace {
bool Compatible(const MeshDraw& a, const MeshDraw& b) {
    return a.mesh_ == b.mesh_ && b.color_[3] >= 1 && a.unlit_ == b.unlit_ &&
           a.double_sided_ == b.double_sided_ && a.metallic_ == b.metallic_ &&
           a.roughness_ == b.roughness_ && a.emissive_ == b.emissive_ &&
           a.textures_ == b.textures_ && a.deformations_ == b.deformations_ &&
           (a.double_sided_ || Mirrored(a.model_) == Mirrored(b.model_));
}
}  // namespace
bool Mirrored(const Matrix4& m) {
    return m[0] * (m[5] * m[10] - m[9] * m[6]) - m[4] * (m[1] * m[10] - m[9] * m[2]) +
                   m[8] * (m[1] * m[6] - m[5] * m[2]) <
           0;
}
BgfxSceneInstances::BgfxSceneInstances() {
    const auto caps = bgfx::getCaps();
    if (!caps || !(caps->supported & BGFX_CAPS_INSTANCING)) return;
    GpuHandle vertex(
            bgfx::createShader(bgfx::copy(kSceneInstanceShader, sizeof(kSceneInstanceShader))));
    GpuHandle fragment(
            bgfx::createShader(bgfx::copy(kSceneFragmentShader, sizeof(kSceneFragmentShader))));
    program_ = GpuHandle(bgfx::createProgram(vertex.Get(), fragment.Get(), false));
}
std::uint32_t BgfxSceneInstances::Bind(std::span<const MeshDraw> draws) const {
    if (!bgfx::isValid(program_.Get()) || draws.size() < 2 || draws.front().color_[3] < 1) return 1;
    constexpr std::uint16_t kStride = 2 * sizeof(Matrix4) + 4 * sizeof(float);
    const auto available =
            bgfx::getAvailInstanceDataBuffer(static_cast<std::uint32_t>(draws.size()), kStride);
    if (available < 2) return 1;
    std::uint32_t count = 1;
    while (count < available && Compatible(draws.front(), draws[count])) ++count;
    if (count < 2) return 1;
    // Borrowed transient bytes belong to bgfx for this frame. Fill synchronously
    // before binding; neither a native pointer nor a transient handle escapes.
    bgfx::InstanceDataBuffer buffer{};
    bgfx::allocInstanceDataBuffer(&buffer, count, kStride);
    for (std::uint32_t index = 0; index < count; ++index) {
        std::memcpy(buffer.data + index * kStride, draws[index].model_.data(), sizeof(Matrix4));
        std::memcpy(buffer.data + index * kStride + sizeof(Matrix4), draws[index].normal_.data(),
                    sizeof(Matrix4));
        std::memcpy(buffer.data + index * kStride + 2 * sizeof(Matrix4), draws[index].color_.data(),
                    4 * sizeof(float));
    }
    bgfx::setInstanceDataBuffer(&buffer);
    return count;
}
}  // namespace rhythm::render::detail
