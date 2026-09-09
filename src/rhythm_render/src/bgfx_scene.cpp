#include "bgfx_scene.h"

#include <array>
#include <numbers>

#include "scene_shader.h"

namespace rhythm::render::detail {
BgfxScene::BgfxScene(std::uint64_t device) : meshes_(device), device_(device) {
    layout_.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Tangent, 4, bgfx::AttribType::Float)
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
    deformations_ = GpuHandle(bgfx::createUniform("u_scene_deform", bgfx::UniformType::Vec4, 4));
    deformation_pivots_ =
            GpuHandle(bgfx::createUniform("u_scene_deform_pivot", bgfx::UniformType::Vec4, 4));
}
MeshHandle BgfxScene::Create(std::span<const MeshVertex> vertices,
                             std::span<const std::uint32_t> indices,
                             std::span<const SkinWeights> skin,
                             std::span<const MorphTarget> morphs) {
    const auto handle = meshes_.Allocate(vertices, indices, skin, morphs);
    try {
        Geometry geometry;
        if (!morphs.empty()) {
            if (!morph_) morph_ = std::make_unique<BgfxSceneMorph>();
            geometry.morph_ = morph_->Create(vertices.size(), skin, morphs);
        } else if (!skin.empty()) {
            if (!skin_) skin_ = std::make_unique<BgfxSceneSkin>();
            geometry.skin_ = skin_->Create(skin);
        }
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
                                          Extent extent, TextureHandle depth_observer,
                                          bgfx::TextureHandle depth) {
    const auto found = targets_.find(target.slot_);
    if (found != targets_.end()) {
        if (found->second.observer_ != target) throw std::logic_error("render.stale_depth_target");
        if (found->second.depth_observer_ == depth_observer)
            return found->second.framebuffer_.Get();
        targets_.erase(found);
    }
    constexpr auto flags = BGFX_TEXTURE_RT | BGFX_TEXTURE_RT_WRITE_ONLY;
    if (!bgfx::isTextureValid(0, false, 1, bgfx::TextureFormat::D24S8, flags))
        throw std::runtime_error("render.depth_format");
    DepthTarget entry;
    entry.observer_ = target;
    entry.depth_observer_ = depth_observer;
    if (!bgfx::isValid(depth)) {
        entry.depth_ = GpuHandle(bgfx::createTexture2D(extent.width_, extent.height_, false, 1,
                                                       bgfx::TextureFormat::D24S8, flags));
        depth = entry.depth_.Get();
    }
    const std::array attachments{color, depth};
    entry.framebuffer_ = GpuHandle(bgfx::createFrameBuffer(
            static_cast<std::uint8_t>(attachments.size()), attachments.data(), false));
    return targets_.emplace(target.slot_, std::move(entry)).first->second.framebuffer_.Get();
}
void BgfxScene::ReleaseTarget(TextureHandle target) noexcept {
    std::erase_if(targets_, [&](const auto& entry) {
        return entry.second.observer_ == target || entry.second.depth_observer_ == target;
    });
}
std::uint32_t BgfxScene::Draw(SceneView context, const SceneDrawList& list, std::uint32_t clear,
                              const SceneTextureResolver& resolve) {
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
    const std::array camera{
            list.camera_position_[0], list.camera_position_[1], list.camera_position_[2],
            static_cast<float>(list.lights_.size() + list.positional_lights_.size())};
    lights_.Set(list);
    std::uint32_t submissions = 0;
    for (std::size_t index = 0; index < list.draws_.size();) {
        const auto& draw = list.draws_[index];
        const auto count = instances_.Bind(std::span(list.draws_).subspan(index));
        const auto& mesh = geometry_.at(draw.mesh_.slot_);
        bgfx::setTransform(draw.model_.data());
        bgfx::setVertexBuffer(0, mesh.vertices_.Get());
        if (mesh.morph_.targets_) {
            morph_->Bind(mesh.morph_, draw);
        } else if (!draw.bones_.empty()) {
            bgfx::setVertexBuffer(1, mesh.skin_.Get());
            skin_->Bind(draw.bones_);
        }
        bgfx::setIndexBuffer(mesh.indices_.Get());
        bgfx::setUniform(color_.Get(), draw.color_.data());
        bgfx::setUniform(normal_.Get(), draw.normal_.data());
        std::array<std::array<float, 4>, 4> deformations{}, pivots{};
        for (std::size_t modifier = 0; modifier < draw.deformations_.size(); ++modifier) {
            const auto& source = draw.deformations_[modifier];
            deformations[modifier] = {source.twist_ * std::numbers::pi_v<float> / 180,
                                      source.taper_, float(source.axis_), 1};
            pivots[modifier] = {source.pivot_[0], source.pivot_[1], source.pivot_[2], 0};
        }
        bgfx::setUniform(deformations_.Get(), deformations.data(), 4);
        bgfx::setUniform(deformation_pivots_.Get(), pivots.data(), 4);
        const std::array material{draw.metallic_, draw.roughness_, draw.unlit_ ? 1.0f : 0.0f,
                                  draw.double_sided_ ? 1.0f : 0.0f};
        const std::array emissive{draw.emissive_[0], draw.emissive_[1], draw.emissive_[2], 0.0f};
        bgfx::setUniform(material_.Get(), material.data());
        bgfx::setUniform(emissive_.Get(), emissive.data());
        bgfx::setUniform(camera_.Get(), camera.data());
        const std::array camera_view{list.camera_backward_[0], list.camera_backward_[1],
                                     list.camera_backward_[2], list.orthographic_ ? 1.0f : 0.0f};
        bgfx::setUniform(camera_view_.Get(), camera_view.data());
        lights_.Bind();
        textures_.Bind(draw.textures_, resolve);
        shadow_.Bind(list.shadow_, resolve);
        environment_.Bind(list.environment_, resolve);
        std::uint64_t state =
                BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_LESS |
                BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_INV_SRC_ALPHA);
        if (draw.color_[3] >= 1) state |= BGFX_STATE_WRITE_Z;
        if (!draw.double_sided_)
            state |= context.invert_ != Mirrored(draw.model_) ? BGFX_STATE_CULL_CCW
                                                              : BGFX_STATE_CULL_CW;
        bgfx::setState(state);
        bgfx::submit(view, draw.surface_program_
                                   ? surfaces_->Bind(*draw.surface_program_, !draw.bones_.empty(),
                                                     mesh.morph_.targets_ != 0, count > 1)
                           : mesh.morph_.targets_ ? morph_->Program(count > 1)
                           : !draw.bones_.empty()
                                   ? skin_->Program(count > 1)
                                   : (count > 1 ? instances_.Program() : program_.Get()));
        index += count;
        ++submissions;
    }
    return submissions;
}
SurfaceProgramHandle BgfxScene::CreateSurface(std::span<const std::uint8_t> artifact) {
    if (!surfaces_) surfaces_ = std::make_unique<BgfxSurfacePrograms>(device_);
    return surfaces_->Create(artifact);
}
void BgfxScene::ReleaseSurface(SurfaceProgramHandle handle) noexcept {
    if (surfaces_) surfaces_->Release(handle);
}
bool BgfxScene::IsValid(SurfaceProgramHandle handle) const {
    return surfaces_ && surfaces_->IsValid(handle);
}
void BgfxScene::Validate(const SceneDrawList& list) const {
    meshes_.Validate(list);
    for (const auto& draw : list.draws_) {
        if (!draw.surface_program_) continue;
        if (!surfaces_) throw std::invalid_argument("render.surface_program_input");
        surfaces_->Validate(*draw.surface_program_);
    }
}
}  // namespace rhythm::render::detail
