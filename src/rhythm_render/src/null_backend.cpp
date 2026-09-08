#include <stdexcept>

#include "backend.h"
#include "gpu_point_store.h"
#include "image_program_store.h"
#include "mesh_store.h"
#include "resource_table.h"
#include "rhythm/render/budget.h"

namespace rhythm::render::detail {
namespace {
class NullBackend final : public Backend {
   public:
    TextureHandle Create(Extent extent, std::span<const std::uint8_t> rgba,
                         TexturePrecision precision) override {
        return resources_.Allocate(extent, rgba, precision);
    }
    void Release(TextureHandle handle) noexcept override { resources_.Release(handle); }
    void Update(TextureHandle handle, std::span<const std::uint8_t> rgba) override {
        resources_.ValidateUpload(handle, rgba);
        if (!in_frame_) throw std::logic_error("render.frame_not_open");
    }
    bool IsValid(TextureHandle handle) const override { return resources_.IsValid(handle); }
    TexturePrecision Precision(TextureHandle handle) const override {
        return resources_.Precision(handle);
    }
    bool SupportsScenes() const override {
        resources_.CheckReady();
        return true;
    }
    bool SupportsGpuPoints() const override {
        resources_.CheckReady();
        return true;  // Contract-only backend, no claim of GPU execution.
    }
    bool SupportsSampleableDepth() const override {
        resources_.CheckReady();
        return true;  // Contract-only backend.
    }
    TextureHandle CreateDepth(Extent extent) override { return resources_.AllocateDepth(extent); }
    void SubmitSceneDepth(TextureHandle color, TextureHandle depth, const SceneDrawList& list,
                          std::uint32_t) override {
        resources_.ValidateSceneDepth(color, depth);
        if (!in_frame_) throw std::logic_error("render.frame_not_open");
        meshes_.Validate(list);
        resources_.ValidateSceneMaterials(color, list, depth);
        resources_.RecordSceneSamples(list);
        if (passes_ >= 240) throw BudgetExceeded(Budget::kPasses);
        resources_.DropDepth(color);
        ++passes_;
        draws_ += static_cast<std::uint32_t>(list.draws_.size());
    }
    GpuPointHandle CreateGpuPoints(std::uint32_t capacity) override {
        resources_.CheckReady();
        return points_.Allocate(capacity);
    }
    ImageProgramHandle CreateImageProgram(std::span<const std::uint8_t> artifact) override {
        resources_.CheckReady();
        return image_programs_.Allocate(artifact);
    }
    void ReleaseImageProgram(ImageProgramHandle handle) noexcept override {
        resources_.CheckThread();
        image_programs_.Release(handle);
    }
    bool IsValid(ImageProgramHandle handle) const override {
        resources_.CheckThread();
        return image_programs_.IsValid(handle);
    }
    void ReleaseGpuPoints(GpuPointHandle handle) noexcept override {
        resources_.CheckThread();
        points_.Release(handle);
    }
    bool IsValid(GpuPointHandle handle) const override {
        resources_.CheckThread();
        return points_.IsValid(handle);
    }
    void UpdateGpuParticles(GpuPointHandle handle, const GpuParticleStep& step) override {
        resources_.CheckReady();
        if (!in_frame_) throw std::logic_error("render.frame_not_open");
        points_.Validate(handle, step);
        if (passes_ >= 240) throw BudgetExceeded(Budget::kPasses);
        points_.Updated(handle);
        ++passes_;
    }
    void SubmitGpuPoints(TextureHandle target, GpuPointHandle handle,
                         const GpuPointStyle& style) override {
        resources_.CheckReady();
        if (!in_frame_) throw std::logic_error("render.frame_not_open");
        if (!resources_.IsRenderTarget(target))
            throw std::invalid_argument("render.gpu_point_target");
        points_.ValidateDraw(handle, style);
        if (passes_ >= 240) throw BudgetExceeded(Budget::kPasses);
        ++passes_;
        ++draws_;
    }
    MeshHandle CreateMesh(std::span<const MeshVertex> vertices,
                          std::span<const std::uint32_t> indices,
                          std::span<const SkinWeights> skin) override {
        resources_.CheckReady();
        return meshes_.Allocate(vertices, indices, skin);
    }
    void ReleaseMesh(MeshHandle handle) noexcept override {
        resources_.CheckThread();
        meshes_.Release(handle);
    }
    bool IsValid(MeshHandle handle) const override {
        resources_.CheckThread();
        return meshes_.IsValid(handle);
    }
    void SubmitScene(TextureHandle target, const SceneDrawList& list, std::uint32_t) override {
        resources_.CheckReady();
        if (!in_frame_) throw std::logic_error("render.frame_not_open");
        if (!resources_.IsRenderTarget(target)) throw std::invalid_argument("render.scene_target");
        meshes_.Validate(list);
        resources_.ValidateSceneMaterials(target, list);
        resources_.RecordSceneSamples(list);
        if (passes_ >= 240) throw BudgetExceeded(Budget::kPasses);
        resources_.ReserveDepth(target);
        ++passes_;
        draws_ += static_cast<std::uint32_t>(list.draws_.size());
    }
    void BeginFrame() override {
        resources_.CheckReady();
        if (in_frame_) throw std::logic_error("render.frame_already_open");
        resources_.BeginFrame();
        in_frame_ = true;
        passes_ = 0;
        draws_ = 0;
    }
    void Submit(TextureHandle target, const DrawList& list, std::uint32_t) override {
        if (!in_frame_) throw std::logic_error("render.frame_not_open");
        resources_.Validate(target, list);
        for (const auto& command : list.commands_)
            if (command.image_program_) image_programs_.Validate(*command.image_program_);
        // Reserve the last 16 views for host/UI presentation after graph admission fails.
        if (passes_ >= (target == TextureHandle{} ? 256U : 240U))
            throw BudgetExceeded(Budget::kPasses);
        resources_.RecordSamples(list);
        ++passes_;
        draws_ += static_cast<std::uint32_t>(list.commands_.size());
    }
    void EndFrame() override {
        resources_.CheckThread();
        if (!in_frame_) throw std::logic_error("render.frame_not_open");
        in_frame_ = false;
        ++frame_;
    }
    FrameStats Stats() const override {
        auto stats = resources_.Stats();
        meshes_.AddStats(stats);
        points_.AddStats(stats);
        image_programs_.AddStats(stats);
        stats.frame_ = frame_;
        stats.passes_ = passes_;
        stats.draws_ = draws_;
        return stats;
    }
    void Invalidate() override {
        if (in_frame_) throw std::logic_error("render.frame_still_open");
        resources_.Invalidate();
        meshes_.Invalidate();
        points_.Invalidate();
        image_programs_.Invalidate();
    }

   private:
    ResourceTable resources_{};
    MeshStore meshes_{resources_.DeviceId()};
    GpuPointStore points_{resources_.DeviceId()};
    ImageProgramStore image_programs_{resources_.DeviceId()};
    bool in_frame_ = false;
    std::uint64_t frame_ = 0;
    std::uint32_t passes_ = 0;
    std::uint32_t draws_ = 0;
};
}  // namespace
std::shared_ptr<Backend> CreateNullBackend() { return std::make_shared<NullBackend>(); }
}  // namespace rhythm::render::detail
