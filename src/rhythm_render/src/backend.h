#pragma once

#include <stdexcept>

#include "rhythm/render/renderer.h"

namespace rhythm::render::detail {
class Backend {
   public:
    virtual ~Backend() = default;
    virtual TextureHandle Create(Extent extent, std::span<const std::uint8_t> rgba,
                                 TexturePrecision precision) = 0;
    virtual void Release(TextureHandle handle) noexcept = 0;
    virtual void Update(TextureHandle handle, std::span<const std::uint8_t> rgba) = 0;
    virtual bool IsValid(TextureHandle handle) const = 0;
    virtual bool SupportsScenes() const { return false; }
    virtual MeshHandle CreateMesh(std::span<const MeshVertex>, std::span<const std::uint32_t>) {
        throw std::logic_error("render.scene_unsupported");
    }
    virtual void ReleaseMesh(MeshHandle) noexcept {}
    virtual bool IsValid(MeshHandle) const { return false; }
    virtual void SubmitScene(TextureHandle, const SceneDrawList&, std::uint32_t) {
        throw std::logic_error("render.scene_unsupported");
    }
    virtual void BeginFrame() = 0;
    virtual void Submit(TextureHandle target, const DrawList& list, std::uint32_t clear) = 0;
    virtual void EndFrame() = 0;
    virtual FrameStats Stats() const = 0;
    virtual void Invalidate() = 0;
};
std::shared_ptr<Backend> CreateNullBackend();
}  // namespace rhythm::render::detail
