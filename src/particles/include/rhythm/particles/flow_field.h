#pragma once

#include <array>
#include <optional>

#include "rhythm/particles/types.h"

namespace rhythm::particles {
// Bounded curl-derived velocity lattice. Evaluated at 30 Hz simulation time,
// interpolated spatially per particle; no per-particle noise-library calls.
class FlowField final {
   public:
    void Prepare(FlowConfig config, std::uint32_t seed, std::uint64_t step);
    std::array<double, 2> Velocity(double x, double y) const;

   private:
    static constexpr std::size_t kSide = 33;
    std::array<std::array<double, 2>, (kSide) * (kSide)> samples_{};
    FlowConfig config_{};
    std::uint32_t seed_ = 0;
    std::optional<std::uint64_t> tick_{};
};
}  // namespace rhythm::particles
