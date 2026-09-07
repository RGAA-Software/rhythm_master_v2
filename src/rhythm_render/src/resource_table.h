#pragma once

#include <thread>

#include "rhythm/render/renderer.h"

namespace rhythm::render::detail {
// Shared validation and budgets used by both Null and GPU adapters.
class ResourceTable final {
   public:
    ResourceTable();
    TextureHandle Allocate(Extent extent, std::span<const std::uint8_t> rgba,
                           TexturePrecision precision);
    void Release(TextureHandle handle) noexcept;
    bool IsValid(TextureHandle handle) const;
    bool Owns(TextureHandle handle) const;
    void Invalidate();
    void CheckReady() const;
    Extent Size(TextureHandle handle) const;
    TexturePrecision Precision(TextureHandle handle) const;
    void ValidateUpload(TextureHandle handle, std::span<const std::uint8_t> rgba) const;
    void BeginFrame();
    void RecordSamples(const DrawList& list);
    void CheckThread() const;
    std::uint64_t DeviceId() const { return device_; }
    bool IsRenderTarget(TextureHandle handle) const;
    bool ReserveDepth(TextureHandle handle);
    void DropDepth(TextureHandle handle) noexcept;
    void Validate(TextureHandle target, const DrawList& list) const;
    FrameStats Stats() const;

   private:
    struct Slot {
        std::uint32_t generation_ = 1;
        Extent extent_{};
        bool live_ = false;
        bool render_target_ = false;
        bool depth_ = false;
        bool sampled_ = false;
        TexturePrecision precision_ = TexturePrecision::kUnorm8;
    };
    std::uint64_t device_ = 0;
    std::vector<Slot> slots_{};
    std::uint64_t bytes_ = 0;
    std::uint32_t live_ = 0;
    bool lost_ = false;
    std::thread::id thread_ = std::this_thread::get_id();
};
}  // namespace rhythm::render::detail
