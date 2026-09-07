#include "rhythm/particles/flow_field.h"

#include <algorithm>
#include <cmath>
#include <glm/gtc/noise.hpp>
#include <stdexcept>

namespace rhythm::particles {
void FlowField::Prepare(FlowConfig config, std::uint32_t seed, std::uint64_t step) {
    if (!std::isfinite(config.strength_) || config.strength_ < 0 || config.strength_ > 2 ||
        !std::isfinite(config.frequency_) || config.frequency_ < 0.25 || config.frequency_ > 12 ||
        !std::isfinite(config.evolution_) || config.evolution_ < 0 || config.evolution_ > 2)
        throw std::invalid_argument("particles.flow_config");
    const auto tick = step / 4;
    if (tick_ == tick && config_ == config && seed_ == seed) return;
    config_ = config;
    seed_ = seed;
    tick_ = tick;
    if (config.strength_ == 0) return;
    const double phase = std::fmod(static_cast<double>(tick) / 30 * config.evolution_, 289.0);
    const glm::dvec3 offset{double(seed % 251), double((seed / 251) % 251),
                            double((seed / 63001) % 251) + phase};
    constexpr double kEpsilon = 0.01;
    for (std::size_t y = 0; y < kSide; ++y)
        for (std::size_t x = 0; x < kSide; ++x) {
            const glm::dvec3 p =
                    offset + glm::dvec3{double(x) / (kSide - 1) * config.frequency_,
                                        double(y) / (kSide - 1) * config.frequency_, 0};
            // Reuse vcpkg GLM's Gustavson/Ashima Perlin implementation. Rotate
            // the scalar potential's gradient, then bound advection speed.
            const double dx = (glm::perlin(p + glm::dvec3{kEpsilon, 0, 0}) -
                               glm::perlin(p - glm::dvec3{kEpsilon, 0, 0})) /
                              (2 * kEpsilon);
            const double dy = (glm::perlin(p + glm::dvec3{0, kEpsilon, 0}) -
                               glm::perlin(p - glm::dvec3{0, kEpsilon, 0})) /
                              (2 * kEpsilon);
            const double amount = config.strength_ / std::max(1.0, std::hypot(dx, dy));
            samples_[y * kSide + x] = {dy * amount, -dx * amount};
        }
}
std::array<double, 2> FlowField::Velocity(double x, double y) const {
    if (!std::isfinite(x) || !std::isfinite(y)) throw std::invalid_argument("particles.flow_point");
    if (!tick_ || config_.strength_ == 0) return {};
    // Outside the canvas, continue the edge velocity; particles still retire by lifetime.
    x = std::clamp(x, 0.0, 1.0) * (kSide - 1);
    y = std::clamp(y, 0.0, 1.0) * (kSide - 1);
    const auto ix = std::min(static_cast<std::size_t>(x), kSide - 2);
    const auto iy = std::min(static_cast<std::size_t>(y), kSide - 2);
    std::array<double, 2> result{};
    for (std::size_t axis = 0; axis < result.size(); ++axis) {
        const auto top = std::lerp(samples_[iy * kSide + ix][axis],
                                   samples_[iy * kSide + ix + 1][axis], x - double(ix));
        const auto bottom = std::lerp(samples_[(iy + 1) * kSide + ix][axis],
                                      samples_[(iy + 1) * kSide + ix + 1][axis], x - double(ix));
        result[axis] = std::lerp(top, bottom, y - double(iy));
    }
    return result;
}
}  // namespace rhythm::particles
