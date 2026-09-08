#pragma once

#include <map>

#include "bgfx_handles.h"
#include "bgfx_scene_instances.h"
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
    MeshHandle Create(std::span<const MeshVertex> vertices, std::span<const std::uint32_t> indices);
    void Release(MeshHandle mesh) noexcept;
    bool IsValid(MeshHandle mesh) const { return meshes_.IsValid(mesh); }
    void Validate(const SceneDrawList& list) const { meshes_.Validate(list); }
    bgfx::FrameBufferHandle Target(TextureHandle target, bgfx::TextureHandle color, Extent extent,
                                   TextureHandle depth_observer = {},
                                   bgfx::TextureHandle depth = BGFX_INVALID_HANDLE);
    void ReleaseTarget(TextureHandle target) noexcept;
    std::uint32_t Draw(SceneView view, const SceneDrawList& list, std::uint32_t clear);
    void AddStats(FrameStats& stats) const { meshes_.AddStats(stats); }
    void Invalidate() { meshes_.Invalidate(); }

   private:
    struct Geometry {
        GpuHandle<bgfx::VertexBufferHandle> vertices_{};
        GpuHandle<bgfx::IndexBufferHandle> indices_{};
    };
    struct DepthTarget {
        TextureHandle observer_{};
        TextureHandle depth_observer_{};
        GpuHandle<bgfx::TextureHandle> depth_{};
        GpuHandle<bgfx::FrameBufferHandle> framebuffer_{};
    };
    MeshStore meshes_{0};
    std::vector<Geometry> geometry_{};
    std::map<std::uint32_t, DepthTarget> targets_{};
    bgfx::VertexLayout layout_{};
    GpuHandle<bgfx::ProgramHandle> program_{};
    BgfxSceneInstances instances_{};
    GpuHandle<bgfx::UniformHandle> color_{};
    GpuHandle<bgfx::UniformHandle> normal_{};
    GpuHandle<bgfx::UniformHandle> material_{};
    GpuHandle<bgfx::UniformHandle> emissive_{};
    GpuHandle<bgfx::UniformHandle> camera_{};
    GpuHandle<bgfx::UniformHandle> camera_view_{};
    GpuHandle<bgfx::UniformHandle> light_directions_{};
    GpuHandle<bgfx::UniformHandle> light_colors_{};
};
}  // namespace rhythm::render::detail
