#pragma once
#include "rhythm/runtime/runtime.h"
namespace rhythm::runtime::detail {
// Owns a derived whole-buffer result on the device thread. Runtime determines
// demand and input-version invalidation; evaluation never mutates its source.
class GpuPointMapPass final {
   public:
    render::GpuPointHandle Evaluate(const graph::Instruction& instruction,
                                    std::span<const NodeOutput> outputs,
                                    render::Renderer& renderer);

   private:
    render::GpuPoints points_{};
    std::uint32_t capacity_ = 0;
};
}  // namespace rhythm::runtime::detail
