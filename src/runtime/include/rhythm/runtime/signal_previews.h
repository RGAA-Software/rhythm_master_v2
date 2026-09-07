#pragma once

#include <array>

#include "rhythm/runtime/runtime.h"

namespace rhythm::runtime {
struct SignalTrace {
    static constexpr std::size_t kCapacity = 120;
    std::array<float, kCapacity> samples_{};
    std::size_t count_ = 0;
    std::size_t offset_ = 0;
    double value_ = 0;
    double sampled_seconds_ = 0;
};
// Host-thread numeric inspection only: no GPU resources or graph mutation.
// Nodes must be scalar/signal outputs of the committed plan and evaluated in
// this frame. Samples follow playback time, so pause holds and seek resets.
class SignalPreviews final {
   public:
    static constexpr std::size_t kMaxPreviews = 8;
    void Capture(const FrameResult& frame, std::span<const graph::NodeId> nodes, double seconds,
                 std::uint64_t reset_generation);
    void Clear();
    const std::map<graph::NodeId, SignalTrace>& Traces() const { return traces_; }

   private:
    std::map<graph::NodeId, SignalTrace> traces_{};
    std::optional<double> last_seconds_{};
    std::uint64_t reset_generation_ = 0;
};
}  // namespace rhythm::runtime
