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
    virtual TexturePrecision Precision(TextureHandle handle) const = 0;
    virtual bool SupportsScenes() const { return false; }
    virtual bool SupportsSampleableDepth() const { return false; }
    virtual TextureHandle CreateDepth(Extent) {
        throw std::logic_error("render.sampleable_depth_unsupported");
    }
    virtual void SubmitSceneDepth(TextureHandle, TextureHandle, const SceneDrawList&,
                                  std::uint32_t) {
        throw std::logic_error("render.sampleable_depth_unsupported");
    }
    virtual bool SupportsGpuPoints() const { return false; }
    virtual ImageProgramTarget ImageTarget() const { return ImageProgramTarget::kWindowsSm5; }
    virtual ImageProgramHandle CreateImageProgram(std::span<const std::uint8_t>) {
        throw std::logic_error("render.image_program_unsupported");
    }
    virtual void ReleaseImageProgram(ImageProgramHandle) noexcept {}
    virtual bool IsValid(ImageProgramHandle) const { return false; }
    virtual GpuPointHandle CreateGpuPoints(std::uint32_t) {
        throw std::logic_error("render.gpu_points_unsupported");
    }
    virtual void ReleaseGpuPoints(GpuPointHandle) noexcept {}
    virtual bool IsValid(GpuPointHandle) const { return false; }
    virtual void UpdateGpuParticles(GpuPointHandle, const GpuParticleStep&) {
        throw std::logic_error("render.gpu_points_unsupported");
    }
    virtual void SubmitGpuPoints(TextureHandle, GpuPointHandle, const GpuPointStyle&) {
        throw std::logic_error("render.gpu_points_unsupported");
    }
    virtual bool SupportsReadback() const { return false; }
    virtual std::uint64_t RequestReadback(TextureHandle) {
        throw std::logic_error("render.readback_unsupported");
    }
    virtual std::optional<ReadbackImage> PollReadback(std::uint64_t) {
        throw std::logic_error("render.readback_unsupported");
    }
    virtual void CancelReadback(std::uint64_t) noexcept {}
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
