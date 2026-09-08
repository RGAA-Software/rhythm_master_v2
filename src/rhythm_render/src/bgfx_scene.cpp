#include "bgfx_scene.h"

#include <array>

#include "scene_shader.h"

namespace rhythm::render::detail {
BgfxScene::BgfxScene(std::uint64_t device) : meshes_(device) {
    layout_.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .end();
    GpuHandle vertex(
            bgfx::createShader(bgfx::copy(kSceneVertexShader, sizeof(kSceneVertexShader))));
    GpuHandle fragment(
            bgfx::createShader(bgfx::copy(kSceneFragmentShader, sizeof(kSceneFragmentShader))));
    program_ = GpuHandle(bgfx::createProgram(vertex.Get(), fragment.Get(), false));
    color_ = GpuHandle(bgfx::createUniform("u_scene_color", bgfx::UniformType::Vec4));
    normal_ = GpuHandle(bgfx::createUniform("u_scene_normal", bgfx::UniformType::Mat4));
    material_ = GpuHandle(bgfx::createUniform("u_scene_material", bgfx::UniformType::Vec4));
    emissive_ = GpuHandle(bgfx::createUniform("u_scene_emissive", bgfx::UniformType::Vec4));
    camera_ = GpuHandle(bgfx::createUniform("u_scene_camera", bgfx::UniformType::Vec4));
    camera_view_ = GpuHandle(bgfx::createUniform("u_scene_view", bgfx::UniformType::Vec4));
    light_directions_ =
            GpuHandle(bgfx::createUniform("u_scene_light_directions", bgfx::UniformType::Vec4, 4));
    light_colors_ =
            GpuHandle(bgfx::createUniform("u_scene_light_colors", bgfx::UniformType::Vec4, 4));
}
MeshHandle BgfxScene::Create(std::span<const MeshVertex> vertices,
                             std::span<const std::uint32_t> indices) {
    const auto handle = meshes_.Allocate(vertices, indices);
    try {
        Geometry geometry;
        geometry.vertices_ = GpuHandle(bgfx::createVertexBuffer(
                bgfx::copy(vertices.data(), static_cast<std::uint32_t>(vertices.size_bytes())),
                layout_));
        geometry.indices_ = GpuHandle(bgfx::createIndexBuffer(
                bgfx::copy(indices.data(), static_cast<std::uint32_t>(indices.size_bytes())),
                BGFX_BUFFER_INDEX32));
        if (geometry_.size() <= handle.slot_) geometry_.resize(handle.slot_ + 1);
        geometry_[handle.slot_] = std::move(geometry);
    } catch (...) {
        meshes_.Release(handle);
        throw;
    }
    return handle;
}
void BgfxScene::Release(MeshHandle mesh) noexcept {
    if (!meshes_.Owns(mesh)) return;
    geometry_[mesh.slot_] = {};
    meshes_.Release(mesh);
}
bgfx::FrameBufferHandle BgfxScene::Target(TextureHandle target, bgfx::TextureHandle color,
                                          Extent extent) {
    const auto found = targets_.find(target.slot_);
    if (found != targets_.end()) {
        if (found->second.observer_ != target) throw std::logic_error("render.stale_depth_target");
        return found->second.framebuffer_.Get();
    }
    constexpr auto flags = BGFX_TEXTURE_RT | BGFX_TEXTURE_RT_WRITE_ONLY;
    if (!bgfx::isTextureValid(0, false, 1, bgfx::TextureFormat::D24S8, flags))
        throw std::runtime_error("render.depth_format");
    DepthTarget entry;
    entry.observer_ = target;
    entry.depth_ = GpuHandle(bgfx::createTexture2D(extent.width_, extent.height_, false, 1,
                                                   bgfx::TextureFormat::D24S8, flags));
    const std::array attachments{color, entry.depth_.Get()};
    entry.framebuffer_ = GpuHandle(bgfx::createFrameBuffer(
            static_cast<std::uint8_t>(attachments.size()), attachments.data(), false));
    return targets_.emplace(target.slot_, std::move(entry)).first->second.framebuffer_.Get();
}
void BgfxScene::ReleaseTarget(TextureHandle target) noexcept { targets_.erase(target.slot_); }
void BgfxScene::Draw(SceneView context, const SceneDrawList& list, std::uint32_t clear) {
    const auto view = context.view_;
    bgfx::setViewMode(view, bgfx::ViewMode::Sequential);
    bgfx::setViewFrameBuffer(view, context.framebuffer_);
    bgfx::setViewRect(view, 0, 0, context.extent_.width_, context.extent_.height_);
    const auto alpha = clear & 255;
    const auto channel = [&](std::uint32_t shift) {
        return (((clear >> shift) & 255) * alpha + 127) / 255;
    };
    // The attachment is packed D24S8. Clear both planes: depth-only clears
    // intermittently reject geometry on the validated Adreno 650 GLES driver.
    bgfx::setViewClear(view, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL,
                       channel(24) << 24 | channel(16) << 16 | channel(8) << 8 | alpha, 1);
    auto projection = list.projection_;
    for (std::size_t column = 0; column < 4; ++column) {
        if (context.invert_) projection[column * 4 + 1] *= -1;
        if (!context.homogeneous_depth_)
            projection[column * 4 + 2] =
                    (projection[column * 4 + 2] + projection[column * 4 + 3]) * 0.5f;
    }
    bgfx::setViewTransform(view, list.view_.data(), projection.data());
    if (list.draws_.empty()) bgfx::touch(view);
    const std::array camera{list.camera_position_[0], list.camera_position_[1],
                            list.camera_position_[2], static_cast<float>(list.lights_.size())};
    std::array<std::array<float, 4>, 4> directions{}, colors{};
    for (std::size_t i = 0; i < list.lights_.size(); ++i) {
        const auto& light = list.lights_[i];
        directions[i] = {light.direction_[0], light.direction_[1], light.direction_[2], 0};
        colors[i] = {light.radiance_[0], light.radiance_[1], light.radiance_[2], 0};
    }
    for (const auto& draw : list.draws_) {
        const auto& mesh = geometry_.at(draw.mesh_.slot_);
        bgfx::setTransform(draw.model_.data());
        bgfx::setVertexBuffer(0, mesh.vertices_.Get());
        bgfx::setIndexBuffer(mesh.indices_.Get());
        bgfx::setUniform(color_.Get(), draw.color_.data());
        bgfx::setUniform(normal_.Get(), draw.normal_.data());
        const std::array material{draw.metallic_, draw.roughness_, draw.unlit_ ? 1.0f : 0.0f,
                                  draw.double_sided_ ? 1.0f : 0.0f};
        const std::array emissive{draw.emissive_[0], draw.emissive_[1], draw.emissive_[2], 0.0f};
        bgfx::setUniform(material_.Get(), material.data());
        bgfx::setUniform(emissive_.Get(), emissive.data());
        bgfx::setUniform(camera_.Get(), camera.data());
        const std::array camera_view{list.camera_backward_[0], list.camera_backward_[1],
                                     list.camera_backward_[2], list.orthographic_ ? 1.0f : 0.0f};
        bgfx::setUniform(camera_view_.Get(), camera_view.data());
        bgfx::setUniform(light_directions_.Get(), directions.data(), 4);
        bgfx::setUniform(light_colors_.Get(), colors.data(), 4);
        std::uint64_t state =
                BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_LESS |
                BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_INV_SRC_ALPHA);
        if (draw.color_[3] >= 1) state |= BGFX_STATE_WRITE_Z;
        const auto& m = draw.model_;
        const auto determinant = m[0] * (m[5] * m[10] - m[9] * m[6]) -
                                 m[4] * (m[1] * m[10] - m[9] * m[2]) +
                                 m[8] * (m[1] * m[6] - m[5] * m[2]);
        if (!draw.double_sided_)
            state |=
                    context.invert_ != (determinant < 0) ? BGFX_STATE_CULL_CCW : BGFX_STATE_CULL_CW;
        bgfx::setState(state);
        bgfx::submit(view, program_.Get());
    }
}
}  // namespace rhythm::render::detail
