#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "rhythm/particles/simulation.h"

namespace {
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void Run() {
    using namespace rhythm::particles;
    Config config;
    Simulation sixty(config);
    Simulation one_twenty(config);
    for (int frame = 0; frame < 180; ++frame) sixty.Advance(1.0 / 60);
    for (int frame = 0; frame < 360; ++frame) one_twenty.Advance(1.0 / 120);
    const PointCloud expected(sixty.Points().begin(), sixty.Points().end());
    Require(!expected.empty() &&
                    PointCloud(one_twenty.Points().begin(), one_twenty.Points().end()) == expected,
            "fixed steps agree across presentation frame rates");
    sixty.Reset();
    for (int frame = 0; frame < 180; ++frame) sixty.Advance(1.0 / 60);
    Require(PointCloud(sixty.Points().begin(), sixty.Points().end()) == expected,
            "seeded reset replays births and trajectories");
    auto invalid = config;
    invalid.rate_ = std::numeric_limits<double>::quiet_NaN();
    bool rejected = false;
    try {
        sixty.Configure(invalid);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    Require(rejected && PointCloud(sixty.Points().begin(), sixty.Points().end()) == expected,
            "invalid configuration preserves simulation");
    rejected = false;
    try {
        sixty.Advance(-1);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    Require(rejected && PointCloud(sixty.Points().begin(), sixty.Points().end()) == expected,
            "invalid time preserves simulation");
    config.capacity_ = 16;
    config.rate_ = 0;
    config.width_ = 0;
    config.height_ = 0;
    config.speed_ = {0, 0};
    config.lifetime_ = {2, 2};
    config.gravity_y_ = 1;
    Simulation falling(config);
    Require(falling.Advance(0, 1, 1).emitted_ == 1, "explicit zero-time burst");
    for (int frame = 0; frame < 120; ++frame) falling.Advance(1.0 / 120);
    const auto point = falling.Points().front();
    Require(std::abs(point.velocity_y_ - 1) < 1e-6 && point.velocity_x_ == 0,
            "published velocity uses normalized canvas units per second");
    const auto expected_y = config.center_y_ + 120.0 * 121 / (2 * 120 * 120);
    Require(std::abs(point.y_ - expected_y) < 1e-6 && std::abs(point.age_ - 0.5) < 1e-6,
            "fixed-step gravity trajectory and normalized lifetime");
    std::uint32_t retired = 0;
    for (int frame = 0; frame < 120; ++frame) retired += falling.Advance(1.0 / 120).retired_;
    Require(retired == 1 && falling.Points().empty(),
            "lifetime retirement counted without physics");
    const auto burst = falling.Advance(0, 1, std::numeric_limits<std::uint32_t>::max());
    Require(burst.emitted_ == 16 && falling.Points().size() == 16, "burst capacity bound");
    const auto limited = falling.Advance(60);
    Require(limited.catch_up_limited_ && limited.steps_ == 16, "long frame has bounded catch-up");
    config.capacity_ = 4;
    falling.Configure(config);
    Require(falling.Points().size() == 4, "capacity reduction immediately bounds live points");
    config.seed_ = 123;
    falling.Configure(config);
    Require(falling.Points().empty(), "seed changes reset the stream");
    config.capacity_ = 1024;
    config.shape_ = EmitterShape::kRing;
    config.width_ = 0.8;
    config.height_ = 0.8;
    falling.Configure(config);
    falling.Advance(0, 1, 1024);
    for (const auto& item : falling.Points()) {
        const auto radius = std::hypot(item.x_ - config.center_x_, item.y_ - config.center_y_);
        Require(std::abs(radius - 0.4) < 1e-6, "ring emitter geometric contract");
    }
    std::cout << "particles: frame-rate determinism, reset, gravity, lifetime, burst/catch-up "
                 "bounds, "
                 "invalid-input preservation and ring geometry passed\n";
}
}  // namespace
int main() {
    try {
        Run();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
