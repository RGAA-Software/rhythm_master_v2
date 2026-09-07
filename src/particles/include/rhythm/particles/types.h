#pragma once

#include <cstdint>
#include <vector>

namespace rhythm::particles {
struct Color {
    float red_ = 1;
    float green_ = 1;
    float blue_ = 1;
    float alpha_ = 1;
    bool operator==(const Color&) const = default;
};
// Canvas-normalized positions; size is a fraction of canvas height. Values are
// owned snapshots, independent of simulation storage and graphics resources.
struct Point {
    std::uint64_t id_ = 0;
    float x_ = 0;
    float y_ = 0;
    float rotation_ = 0;
    float size_ = 0;
    float age_ = 0;
    Color color_{};
    // Normalized canvas units per second; angular velocity is radians/second.
    float velocity_x_ = 0;
    float velocity_y_ = 0;
    float angular_velocity_ = 0;
    bool operator==(const Point&) const = default;
};
using PointCloud = std::vector<Point>;
inline constexpr std::uint32_t kMaximumPoints = 16384;
struct Range {
    double minimum_ = 0;
    double maximum_ = 0;
    bool operator==(const Range&) const = default;
};
enum class EmitterShape { kRectangle, kDisk, kRing };
struct Config {
    std::uint32_t capacity_ = 2048;
    std::uint32_t seed_ = 1;
    double rate_ = 120;
    Range lifetime_{1.5, 3};
    EmitterShape shape_ = EmitterShape::kRectangle;
    double center_x_ = 0.5;
    double center_y_ = 0.7;
    double width_ = 0.1;
    double height_ = 0.05;
    double direction_ = -1.5707963267948966;
    double spread_ = 0.5;
    Range speed_{0.1, 0.3};
    double gravity_x_ = 0;
    double gravity_y_ = 0.1;
    Range size_{0.005, 0.015};
    Range angular_speed_{-1, 1};
    Color color_from_{0.1f, 0.8f, 1, 1};
    Color color_to_{0.8f, 0.1f, 1, 0};
    double fade_in_ = 0.05;
    double fade_out_ = 0.25;
    bool operator==(const Config&) const = default;
};
struct Update {
    std::uint32_t emitted_ = 0;
    std::uint32_t retired_ = 0;
    std::uint32_t steps_ = 0;
    bool catch_up_limited_ = false;
};
}  // namespace rhythm::particles
