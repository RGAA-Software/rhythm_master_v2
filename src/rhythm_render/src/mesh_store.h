#pragma once

#include "rhythm/render/renderer.h"

namespace rhythm::render::detail {
// CPU handle/budget validation shared by Null and GPU implementations. The host
// backend checks thread affinity before every call, including owner release.
class MeshStore final {
   public:
    explicit MeshStore(std::uint64_t device) : device_(device) {}
    MeshHandle Allocate(std::span<const MeshVertex> vertices,
                        std::span<const std::uint32_t> indices,
                        std::span<const SkinWeights> skin = {},
                        std::span<const MorphTarget> morphs = {});
    void Release(MeshHandle handle) noexcept;
    bool Owns(MeshHandle handle) const;
    bool IsValid(MeshHandle handle) const { return !lost_ && Owns(handle); }
    void Validate(const SceneDrawList& list) const;
    void AddStats(FrameStats& stats) const;
    void Invalidate() { lost_ = true; }

   private:
    struct Slot {
        std::uint32_t generation_ = 1;
        std::uint32_t indices_ = 0;
        std::uint64_t bytes_ = 0;
        bool live_ = false;
        bool tangents_ = false;
        std::uint8_t bones_ = 0;
        std::uint8_t morphs_ = 0;
    };
    std::uint64_t device_ = 0;
    std::vector<Slot> slots_{};
    std::uint64_t bytes_ = 0;
    std::uint32_t live_ = 0;
    bool lost_ = false;
};
}  // namespace rhythm::render::detail
