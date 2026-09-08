#pragma once

#include "rhythm/runtime/runtime.h"

namespace rhythm::runtime::detail {
bool SamePlan(const graph::ExecutionPlan& left, const graph::ExecutionPlan& right);

// Host-thread schedule for the current plan. Static caches, history, aliasing
// operators and explicitly observed outputs remain persistent. Only ordinary
// dynamic texture targets may be recycled after their final synchronous consumer.
class TextureLifetimes final {
   public:
    void Prepare(const graph::ExecutionPlan& plan,
                 const std::optional<std::vector<graph::NodeId>>& retained);
    bool Enabled() const { return retained_.has_value(); }
    bool Primary(std::size_t index) const { return primary_.at(index); }
    bool Recyclable(std::size_t index) const { return recyclable_.at(index); }
    std::span<const std::size_t> RetireAfter(std::size_t index) const { return retire_.at(index); }
    std::uint64_t Generation() const { return generation_; }

   private:
    std::optional<graph::ExecutionPlan> plan_{};
    std::optional<std::vector<graph::NodeId>> retained_{};
    std::vector<bool> primary_{};
    std::vector<bool> recyclable_{};
    std::vector<std::vector<std::size_t>> retire_{};
    std::uint64_t generation_ = 0;
};

// Move-only Texture owners circulate between states and the free list. Exact
// extents and chosen precision are preserved. Unused free entries retire each
// evaluated frame, so a previous large scene cannot hold an idle high-water pool.
class TexturePool final {
   public:
    void BeginFrame() { ++epoch_; }
    render::Texture Acquire(render::Extent extent, render::Renderer& renderer,
                            render::TexturePrecision precision);
    void Recycle(render::Texture texture, render::Extent extent,
                 render::TexturePrecision precision);
    void EndFrame();

   private:
    struct Entry {
        render::Texture texture_{};
        render::Extent extent_{};
        render::TexturePrecision precision_ = render::TexturePrecision::kUnorm8;
        std::uint64_t epoch_ = 0;
    };
    std::vector<Entry> free_{};
    std::uint64_t epoch_ = 0;
};
}  // namespace rhythm::runtime::detail
