#pragma once

#include "rhythm/render/renderer.h"

namespace rhythm::render::detail {
// Resource admission shared with Null; a concrete backend additionally validates
// and compiles the target artifact before exposing a usable program.
class SurfaceProgramStore final {
   public:
    explicit SurfaceProgramStore(std::uint64_t device) : device_(device) {}
    SurfaceProgramHandle Allocate(std::span<const std::uint8_t> artifact);
    void Release(SurfaceProgramHandle handle) noexcept;
    bool Owns(SurfaceProgramHandle handle) const;
    bool IsValid(SurfaceProgramHandle handle) const { return !lost_ && Owns(handle); }
    void Validate(const SurfaceProgramInput& input) const;
    void AddStats(FrameStats& stats) const;
    void Invalidate() { lost_ = true; }

   private:
    struct Slot {
        std::uint32_t generation_ = 1;
        std::uint32_t bytes_ = 0;
    };
    std::uint64_t device_ = 0;
    std::vector<Slot> slots_{};
    std::uint64_t bytes_ = 0;
    bool lost_ = false;
};
}  // namespace rhythm::render::detail
