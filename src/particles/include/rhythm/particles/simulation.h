#pragma once

#include <span>

#include "rhythm/particles/types.h"

namespace rhythm::particles {
// Single-owner, host-thread simulation. Fixed 120 Hz steps, at most 16 steps per
// call. Excess elapsed time is dropped and reported; callers must not equate
// bounded real-time catch-up with deterministic arbitrary-time seeking.
class Simulation final {
   public:
    explicit Simulation(Config config = {});
    void Configure(const Config& config);
    Update Advance(double seconds, double emission_scale = 1, std::uint32_t burst = 0);
    void Reset();
    std::span<const Point> Points() const { return points_; }
    const Config& Configuration() const { return config_; }

   private:
    struct State {
        std::uint64_t id_ = 0;
        double x_ = 0;
        double y_ = 0;
        double velocity_x_ = 0;
        double velocity_y_ = 0;
        double age_ = 0;
        double lifetime_ = 1;
        double rotation_ = 0;
        double angular_speed_ = 0;
        double size_ = 0;
    };
    double Unit();
    double Sample(Range range);
    void Emit(std::uint32_t count, Update& update);
    void Step(Update& update);
    void Publish();
    Config config_{};
    std::vector<State> states_{};
    PointCloud points_{};
    std::uint64_t random_ = 1;
    std::uint64_t next_id_ = 1;
    double accumulator_ = 0;
    double emission_ = 0;
};
}  // namespace rhythm::particles
