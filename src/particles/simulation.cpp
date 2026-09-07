// Adapted from the project's previous portable particle simulation.
// Exact source hashes and adaptation scope: provenance/particles.json.
#include "rhythm/particles/simulation.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace rhythm::particles {
namespace {
constexpr double kStep = 1.0 / 120;
constexpr std::uint32_t kMaximumSteps = 16;
bool Between(double value, double minimum, double maximum) {
    return std::isfinite(value) && value >= minimum && value <= maximum;
}
bool Valid(Range range, double minimum, double maximum) {
    return Between(range.minimum_, minimum, maximum) &&
           Between(range.maximum_, range.minimum_, maximum);
}
bool Valid(Color color) {
    return Between(color.red_, 0, 1) && Between(color.green_, 0, 1) && Between(color.blue_, 0, 1) &&
           Between(color.alpha_, 0, 1);
}
void Validate(const Config& config) {
    if (!config.capacity_ || config.capacity_ > kMaximumPoints ||
        !Between(config.rate_, 0, 20000) || !Valid(config.lifetime_, 0.05, 60) ||
        !Between(config.center_x_, -4, 4) || !Between(config.center_y_, -4, 4) ||
        !Between(config.width_, 0, 4) || !Between(config.height_, 0, 4) ||
        !Between(config.direction_, -100, 100) ||
        !Between(config.spread_, 0, 2 * std::numbers::pi) || !Valid(config.speed_, 0, 10) ||
        !Between(config.gravity_x_, -20, 20) || !Between(config.gravity_y_, -20, 20) ||
        !Valid(config.size_, 0, 1) || !Valid(config.angular_speed_, -100, 100) ||
        !Valid(config.color_from_) || !Valid(config.color_to_) || !Between(config.fade_in_, 0, 1) ||
        !Between(config.fade_out_, 0, 1) ||
        (config.shape_ != EmitterShape::kRectangle && config.shape_ != EmitterShape::kDisk &&
         config.shape_ != EmitterShape::kRing))
        throw std::invalid_argument("particles.config");
}
}  // namespace
Simulation::Simulation(Config config) {
    Configure(config);
    Reset();
}
void Simulation::Configure(const Config& config) {
    Validate(config);
    const bool reset = config.seed_ != config_.seed_;
    // Reserve before installing the new configuration; allocation failure keeps
    // the previous simulation's logical state intact.
    states_.reserve(config.capacity_);
    points_.reserve(config.capacity_);
    config_ = config;
    if (reset) Reset();
    if (states_.size() > config.capacity_) states_.resize(config.capacity_);
    Publish();
}
void Simulation::Reset() {
    states_.clear();
    points_.clear();
    random_ = config_.seed_ ? config_.seed_ : 0x9e3779b97f4a7c15ULL;
    next_id_ = 1;
    accumulator_ = 0;
    emission_ = 0;
}
double Simulation::Unit() {
    auto value = random_;
    value ^= value >> 12;
    value ^= value << 25;
    value ^= value >> 27;
    random_ = value;
    return static_cast<double>((value * 0x2545f4914f6cdd1dULL) >> 40) / 16777216;
}
double Simulation::Sample(Range range) { return std::lerp(range.minimum_, range.maximum_, Unit()); }
void Simulation::Emit(std::uint32_t count, Update& update) {
    count = std::min(count, config_.capacity_ - static_cast<std::uint32_t>(states_.size()));
    if (count > std::numeric_limits<std::uint64_t>::max() - next_id_)
        throw std::overflow_error("particles.id_exhausted");
    for (std::uint32_t index = 0; index < count; ++index) {
        State state;
        state.id_ = next_id_++;
        if (config_.shape_ == EmitterShape::kRectangle) {
            state.x_ = config_.center_x_ + (Unit() - 0.5) * config_.width_;
            state.y_ = config_.center_y_ + (Unit() - 0.5) * config_.height_;
        } else {
            const auto angle = Unit() * 2 * std::numbers::pi;
            const auto radius =
                    config_.shape_ == EmitterShape::kRing ? 0.5 : 0.5 * std::sqrt(Unit());
            state.x_ = config_.center_x_ + std::cos(angle) * radius * config_.width_;
            state.y_ = config_.center_y_ + std::sin(angle) * radius * config_.height_;
        }
        const auto angle = config_.direction_ + (Unit() - 0.5) * config_.spread_;
        const auto speed = Sample(config_.speed_);
        state.velocity_x_ = std::cos(angle) * speed;
        state.velocity_y_ = std::sin(angle) * speed;
        state.lifetime_ = Sample(config_.lifetime_);
        state.size_ = Sample(config_.size_);
        state.rotation_ = Unit() * 2 * std::numbers::pi;
        state.angular_speed_ = Sample(config_.angular_speed_);
        states_.push_back(state);
    }
    update.emitted_ += count;
}
void Simulation::Step(Update& update) {
    for (std::size_t index = 0; index < states_.size();) {
        auto& state = states_[index];
        state.age_ += kStep;
        if (state.age_ + 1e-12 >= state.lifetime_) {
            state = states_.back();
            states_.pop_back();
            ++update.retired_;
            continue;
        }
        state.velocity_x_ += config_.gravity_x_ * kStep;
        state.velocity_y_ += config_.gravity_y_ * kStep;
        state.x_ += state.velocity_x_ * kStep;
        state.y_ += state.velocity_y_ * kStep;
        state.rotation_ += state.angular_speed_ * kStep;
        ++index;
    }
}
void Simulation::Publish() {
    points_.clear();
    for (const auto& state : states_) {
        const auto age = std::clamp(state.age_ / state.lifetime_, 0.0, 1.0);
        const auto fade_in = config_.fade_in_ > 0 ? std::min(1.0, age / config_.fade_in_) : 1;
        const auto fade_out =
                config_.fade_out_ > 0 ? std::min(1.0, (1 - age) / config_.fade_out_) : 1;
        const auto mix = [&](float first, float second) {
            return std::lerp(first, second, static_cast<float>(age));
        };
        const auto& first = config_.color_from_;
        const auto& second = config_.color_to_;
        Color color{mix(first.red_, second.red_), mix(first.green_, second.green_),
                    mix(first.blue_, second.blue_),
                    mix(first.alpha_, second.alpha_) * static_cast<float>(fade_in * fade_out)};
        points_.push_back({state.id_, static_cast<float>(state.x_), static_cast<float>(state.y_),
                           static_cast<float>(state.rotation_), static_cast<float>(state.size_),
                           static_cast<float>(age), color, static_cast<float>(state.velocity_x_),
                           static_cast<float>(state.velocity_y_),
                           static_cast<float>(state.angular_speed_)});
    }
}
Update Simulation::Advance(double seconds, double emission_scale, std::uint32_t burst) {
    if (!Between(seconds, 0, 60) || !Between(emission_scale, 0, 100))
        throw std::invalid_argument("particles.frame");
    Update result;
    Emit(burst, result);
    constexpr double maximum = kMaximumSteps * kStep;
    result.catch_up_limited_ = seconds > maximum;
    accumulator_ += std::min(seconds, maximum);
    while (accumulator_ + 1e-12 >= kStep && result.steps_ < kMaximumSteps) {
        emission_ = std::min(emission_ + config_.rate_ * emission_scale * kStep,
                             static_cast<double>(config_.capacity_));
        const auto count = static_cast<std::uint32_t>(std::floor(emission_ + 1e-12));
        emission_ = std::max(0.0, emission_ - count);
        Emit(count, result);
        Step(result);
        accumulator_ = std::max(0.0, accumulator_ - kStep);
        ++result.steps_;
    }
    Publish();
    return result;
}
}  // namespace rhythm::particles
