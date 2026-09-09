#pragma once

#include "rhythm/render/renderer.h"

namespace rhythm::render::detail {
// Shared admission and handle-generation invariants; host adapter checks thread.
class GpuPointStore final {
   public:
    explicit GpuPointStore(std::uint64_t device) : device_(device) {}
    GpuPointHandle Allocate(std::uint32_t capacity);
    void Release(GpuPointHandle handle) noexcept;
    bool Owns(GpuPointHandle handle) const;
    bool IsValid(GpuPointHandle handle) const { return !lost_ && Owns(handle); }
    std::uint32_t Capacity(GpuPointHandle handle) const;
    void Validate(GpuPointHandle handle, const GpuParticleStep& step) const;
    void ValidateMap(GpuPointHandle source, GpuPointHandle destination,
                     const GpuPointMapping& mapping) const;
    void Updated(GpuPointHandle handle);
    void ValidateDraw(GpuPointHandle handle, const GpuPointStyle& style) const;
    void AddStats(FrameStats& stats) const;
    void Invalidate() { lost_ = true; }

   private:
    struct Slot {
        std::uint32_t generation_ = 1;
        std::uint32_t capacity_ = 0;
        bool initialized_ = false;
    };
    std::uint64_t device_ = 0;
    std::vector<Slot> slots_{};
    std::uint32_t total_ = 0;
    bool lost_ = false;
};
}  // namespace rhythm::render::detail
