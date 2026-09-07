#include <stdexcept>

#include "backend.h"
#include "mesh_store.h"
#include "resource_table.h"

namespace rhythm::render::detail {
namespace {
class NullBackend final : public Backend {
   public:
    TextureHandle Create(Extent extent, std::span<const std::uint8_t> rgba) override {
        return resources_.Allocate(extent, rgba);
    }
    void Release(TextureHandle handle) noexcept override { resources_.Release(handle); }
    bool IsValid(TextureHandle handle) const override { return resources_.IsValid(handle); }
    bool SupportsScenes() const override {
        resources_.CheckReady();
        return true;
    }
    MeshHandle CreateMesh(std::span<const MeshVertex> vertices,
                          std::span<const std::uint32_t> indices) override {
        resources_.CheckReady();
        return meshes_.Allocate(vertices, indices);
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
        if (passes_ >= 240) throw std::length_error("render.pass_limit");
        resources_.ReserveDepth(target);
        ++passes_;
        draws_ += static_cast<std::uint32_t>(list.draws_.size());
    }
    void BeginFrame() override {
        resources_.CheckReady();
        if (in_frame_) throw std::logic_error("render.frame_already_open");
        in_frame_ = true;
        passes_ = 0;
        draws_ = 0;
    }
    void Submit(TextureHandle target, const DrawList& list, std::uint32_t) override {
        if (!in_frame_) throw std::logic_error("render.frame_not_open");
        resources_.Validate(target, list);
        if (passes_ >= 240) throw std::length_error("render.pass_limit");
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
        stats.frame_ = frame_;
        stats.passes_ = passes_;
        stats.draws_ = draws_;
        return stats;
    }
    void Invalidate() override {
        if (in_frame_) throw std::logic_error("render.frame_still_open");
        resources_.Invalidate();
        meshes_.Invalidate();
    }

   private:
    ResourceTable resources_{};
    MeshStore meshes_{resources_.DeviceId()};
    bool in_frame_ = false;
    std::uint64_t frame_ = 0;
    std::uint32_t passes_ = 0;
    std::uint32_t draws_ = 0;
};
}  // namespace
std::shared_ptr<Backend> CreateNullBackend() { return std::make_shared<NullBackend>(); }
}  // namespace rhythm::render::detail
