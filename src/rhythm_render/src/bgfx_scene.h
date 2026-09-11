#pragma once

#include <map>
#include <memory>

#include "bgfx_handles.h"
#include "bgfx_scene_environment.h"
#include "bgfx_scene_instances.h"
#include "bgfx_scene_lights.h"
#include "bgfx_scene_morph.h"
#include "bgfx_scene_shadow.h"
#include "bgfx_scene_skin.h"
#include "bgfx_scene_textures.h"
#include "bgfx_surface_programs.h"
#include "mesh_store.h"

namespace rhythm::render::detail {
struct SceneView {
    bgfx::ViewId view_ = 0;
    bgfx::FrameBufferHandle framebuffer_ = BGFX_INVALID_HANDLE;
    Extent extent_{};
    bool invert_ = false;
    bool homogeneous_depth_ = false;
};
// Owns scene programs, immutable mesh buffers and depth attachments. Native
// color textures are borrowed from BgfxBackend and released there afterward.
class BgfxScene final {
   public:
    explicit BgfxScene(std::uint64_t device);
    MeshHandle Create(std::span<const MeshVertex> vertices, std::span<const std::uint32_t> indices,
                      std::span<const SkinWeights> skin, std::span<const MorphTarget> morphs);
    void Release(MeshHandle mesh) noexcept;
    bool IsValid(MeshHandle mesh) const { return meshes_.IsValid(mesh); }
    void Validate(const SceneDrawList& list) const;
    SurfaceProgramHandle CreateSurface(std::span<const std::uint8_t> artifact);
    void ReleaseSurface(SurfaceProgramHandle handle) noexcept;
    bool IsValid(SurfaceProgramHandle handle) const;
    bgfx::FrameBufferHandle Target(TextureHandle target, bgfx::TextureHandle color, Extent extent,
                                   TextureHandle depth_observer = {},
                                   bgfx::TextureHandle depth = BGFX_INVALID_HANDLE);
    void ReleaseTarget(TextureHandle target) noexcept;
    std::uint32_t Draw(SceneView view, const SceneDrawList& list, std::uint32_t clear,
                       const SceneTextureResolver& resolve);
    void AddStats(FrameStats& stats) const {
        meshes_.AddStats(stats);
        if (surfaces_) surfaces_->AddStats(stats);
    }
    void Invalidate() {
        meshes_.Invalidate();
        if (surfaces_) surfaces_->Invalidate();
    }

   private:
    struct Geometry {
        GpuHandle<bgfx::VertexBufferHandle> vertices_{};
        GpuHandle<bgfx::IndexBufferHandle> indices_{};
        GpuHandle<bgfx::VertexBufferHandle> skin_{};
        MorphGeometry morph_{};
    };
    struct DepthTarget {
        TextureHandle observer_{};
        TextureHandle depth_observer_{};
        GpuHandle<bgfx::TextureHandle> depth_{};
        GpuHandle<bgfx::FrameBufferHandle> framebuffer_{};
    };
    MeshStore meshes_{0};
    std::uint64_t device_ = 0;
    std::unique_ptr<BgfxSurfacePrograms> surfaces_{};
    std::vector<Geometry> geometry_{};
    std::map<std::uint32_t, DepthTarget> targets_{};
    bgfx::VertexLayout layout_{};
    GpuHandle<bgfx::ProgramHandle> program_{};
    std::array<GpuHandle<bgfx::ProgramHandle>, 3> depth_prepass_programs_{};
    BgfxSceneInstances instances_{};
    std::unique_ptr<BgfxSceneSkin> skin_{};
    std::unique_ptr<BgfxSceneMorph> morph_{};
    GpuHandle<bgfx::UniformHandle> color_{};
    GpuHandle<bgfx::UniformHandle> normal_{};
    GpuHandle<bgfx::UniformHandle> material_{};
    GpuHandle<bgfx::UniformHandle> emissive_{};
    GpuHandle<bgfx::UniformHandle> camera_{};
    GpuHandle<bgfx::UniformHandle> camera_view_{};
    GpuHandle<bgfx::UniformHandle> alpha_{};
    GpuHandle<bgfx::UniformHandle> deformations_{};
    GpuHandle<bgfx::UniformHandle> deformation_pivots_{};
    BgfxSceneLights lights_{};
    BgfxSceneTextures textures_{};
    BgfxSceneShadow shadow_{};
    BgfxSceneEnvironment environment_{};
};
}  // namespace rhythm::render::detail
