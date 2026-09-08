#pragma once
#include "rhythm/runtime/runtime.h"
namespace rhythm::runtime::detail {
// Fixed-step, host-thread clock/ring owner. Point records remain GPU-only.
// Backward time or a forward discontinuity (> 8 steps) restarts at the requested
// time; this is a seeded reset, not reconstruction of skipped simulation history.
class GpuParticlePass final {
   public:
    render::GpuPointHandle Evaluate(const graph::Instruction& instruction,
                                    std::span<const NodeOutput> outputs, FrameContext frame,
                                    render::Renderer& renderer);

   private:
    render::GpuPoints points_{};
    std::optional<graph::Node> configuration_{};
    std::optional<double> last_seconds_{};
    double accumulator_ = 0;
    double emission_fraction_ = 0;
    std::uint32_t capacity_ = 0;
    std::uint32_t cursor_ = 0;
    std::uint32_t sequence_ = 0;
    bool burst_high_ = false;
};
}  // namespace rhythm::runtime::detail
