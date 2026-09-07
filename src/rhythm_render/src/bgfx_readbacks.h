#pragma once

#include <array>
#include <memory>

#include "bgfx_handles.h"
#include "resource_table.h"

namespace rhythm::render::detail {
// Owned by the backend before its DeviceLifetime member: shutdown must complete
// native writes even when every ticket was canceled without another EndFrame.
struct ReadbackMemory {
    std::array<ReadbackImage, 3> images_{};
};

class BgfxReadbacks final {
   public:
    explicit BgfxReadbacks(std::shared_ptr<ReadbackMemory> memory);
    static bool Supported();
    std::uint64_t Request(bgfx::TextureHandle source, Extent extent, bgfx::ViewId view,
                          ResourceTable& resources);
    void Advance(std::uint32_t completed, ResourceTable& resources);
    std::optional<ReadbackImage> Poll(std::uint64_t ticket, ResourceTable& resources);
    void Cancel(std::uint64_t ticket, ResourceTable& resources) noexcept;
    void Invalidate(ResourceTable& resources);

   private:
    enum class Stage { kEmpty, kBlit, kRead, kReady };
    struct Slot {
        GpuHandle<bgfx::TextureHandle> texture_{};
        TextureHandle budget_{};
        std::uint64_t ticket_ = 0;
        std::uint32_t ready_ = 0;
        Stage stage_ = Stage::kEmpty;
        bool canceled_ = false;
    };
    void Release(std::size_t index, ResourceTable& resources);
    std::shared_ptr<ReadbackMemory> memory_{};
    std::array<Slot, 3> slots_{};
    std::uint64_t next_ticket_ = 1;
};
}  // namespace rhythm::render::detail
