#include "point_ops.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace rhythm::runtime::detail {
namespace {
particles::Color Color(graph::Color value) {
    return {static_cast<float>(value.r_), static_cast<float>(value.g_),
            static_cast<float>(value.b_), static_cast<float>(value.a_)};
}
double Control(const graph::Instruction& instruction, std::span<const NodeOutput> outputs,
               std::size_t port, std::string_view property, double fallback, double minimum,
               double maximum) {
    const double value = instruction.inputs_.at(port)
                                 ? outputs[*instruction.inputs_[port]].scalar_
                                 : graph::Scalar(instruction.node_, property, fallback);
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}
particles::Config Configuration(const graph::Node& node) {
    const auto scalar = [&](std::string_view key, double fallback) {
        return graph::Scalar(node, key, fallback);
    };
    particles::Config config;
    config.capacity_ = static_cast<std::uint32_t>(scalar("particle_capacity", 2048));
    config.seed_ = static_cast<std::uint32_t>(scalar("seed", 1));
    config.rate_ = scalar("emission_rate", 120);
    const auto lifetime = scalar("lifetime", 3);
    config.lifetime_ = {std::max(0.05, lifetime * (1 - scalar("lifetime_variation", 0.5))),
                        lifetime};
    config.shape_ =
            static_cast<particles::EmitterShape>(static_cast<int>(scalar("emitter_shape", 0)));
    config.center_x_ = scalar("center_x", 0.5);
    config.center_y_ = scalar("center_y", 0.7);
    config.width_ = scalar("emitter_width", 0.1);
    config.height_ = scalar("emitter_height", 0.05);
    config.direction_ = scalar("direction", -90) * std::numbers::pi / 180;
    config.spread_ = scalar("spread", 30) * std::numbers::pi / 180;
    const auto speed = scalar("particle_speed", 0.3);
    config.speed_ = {speed * (1 - scalar("speed_variation", 0.67)), speed};
    config.gravity_x_ = scalar("gravity_x", 0);
    config.gravity_y_ = scalar("gravity_y", 0.1);
    const auto size = scalar("point_size", 0.015);
    config.size_ = {size * (1 - scalar("size_variation", 0.67)), size};
    const auto angular = scalar("angular_speed", 1);
    config.angular_speed_ = {-angular, angular};
    config.fade_in_ = scalar("fade_in", 0.05);
    config.fade_out_ = scalar("fade_out", 0.25);
    config.color_from_ = Color(graph::ColorValue(node, "color_a", {0.1, 0.8, 1, 1}));
    config.color_to_ = Color(graph::ColorValue(node, "color_b", {0.8, 0.1, 1, 0}));
    return config;
}
}  // namespace
std::shared_ptr<const particles::PointCloud> EmitPoints(PointState& state,
                                                        const graph::Instruction& instruction,
                                                        std::span<const NodeOutput> outputs,
                                                        FrameContext frame) {
    const auto config = Configuration(instruction.node_);
    if (config != state.simulation_.Configuration()) state.simulation_.Configure(config);
    if (state.last_seconds_ && frame.seconds_ < *state.last_seconds_) {
        state.simulation_.Reset();
        state.last_seconds_.reset();
        state.burst_high_ = false;
    }
    const auto delta =
            state.last_seconds_ ? std::clamp(frame.seconds_ - *state.last_seconds_, 0.0, 60.0) : 0;
    const auto burst = Control(instruction, outputs, 1, "burst", 0, 0, particles::kMaximumPoints);
    if (frame.advance_state_) {
        const auto update = state.simulation_.Advance(
                delta, Control(instruction, outputs, 0, "emission", 1, 0, 100),
                burst > 0 && !state.burst_high_ ? static_cast<std::uint32_t>(burst) : 0);
        state.catch_up_limited_ = update.catch_up_limited_;
        state.burst_high_ = burst > 0;
    }
    state.last_seconds_ = frame.seconds_;
    const auto points = state.simulation_.Points();
    return std::make_shared<const particles::PointCloud>(points.begin(), points.end());
}
std::shared_ptr<const particles::PointCloud> GridPoints(const graph::Node& node) {
    const auto columns = static_cast<std::uint32_t>(graph::Scalar(node, "columns", 16));
    const auto rows = static_cast<std::uint32_t>(graph::Scalar(node, "rows", 9));
    if (!columns || !rows || columns > 128 || rows > 128)
        throw std::invalid_argument("runtime.points");
    particles::PointCloud points;
    points.reserve(columns * rows);
    const auto width = graph::Scalar(node, "grid_width", 0.8);
    const auto height = graph::Scalar(node, "grid_height", 0.8);
    const auto x = graph::Scalar(node, "center_x", 0.5);
    const auto y = graph::Scalar(node, "center_y", 0.5);
    const auto size = static_cast<float>(graph::Scalar(node, "point_size", 0.012));
    const auto color = Color(graph::ColorValue(node, "color_a", {0.1, 0.8, 1, 1}));
    for (std::uint32_t row = 0; row < rows; ++row)
        for (std::uint32_t column = 0; column < columns; ++column)
            points.push_back(
                    {points.size() + 1,
                     static_cast<float>(
                             x + (columns == 1
                                          ? 0
                                          : static_cast<double>(column) / (columns - 1) - 0.5) *
                                         width),
                     static_cast<float>(
                             y + (rows == 1 ? 0 : static_cast<double>(row) / (rows - 1) - 0.5) *
                                         height),
                     0, size, 0, color});
    return std::make_shared<const particles::PointCloud>(std::move(points));
}
std::shared_ptr<const particles::PointCloud> TransformPoints(const graph::Instruction& instruction,
                                                             std::span<const NodeOutput> outputs,
                                                             double aspect) {
    if (!std::isfinite(aspect) || aspect <= 0) throw std::invalid_argument("runtime.points");
    const auto& source = outputs[instruction.inputs_.at(0).value()].points_;
    if (!source || source->size() > particles::kMaximumPoints)
        throw std::invalid_argument("runtime.points");
    particles::PointCloud points = *source;
    const auto scale = Control(instruction, outputs, 1, "scale", 1, 0, 8);
    const auto rotation =
            Control(instruction, outputs, 2, "rotation", 0, -36000, 36000) * std::numbers::pi / 180;
    const auto opacity = Control(instruction, outputs, 3, "opacity", 1, 0, 1);
    const auto cosine = std::cos(rotation);
    const auto sine = std::sin(rotation);
    const auto dx = graph::Scalar(instruction.node_, "translate_x", 0);
    const auto dy = graph::Scalar(instruction.node_, "translate_y", 0);
    for (auto& point : points) {
        const auto x = (point.x_ - 0.5) * scale * aspect;
        const auto y = (point.y_ - 0.5) * scale;
        point.x_ = static_cast<float>(
                std::clamp(0.5 + (cosine * x - sine * y) / aspect + dx, -1e6, 1e6));
        point.y_ = static_cast<float>(std::clamp(0.5 + sine * x + cosine * y + dy, -1e6, 1e6));
        point.rotation_ = static_cast<float>(
                std::remainder(point.rotation_ + rotation, 2 * std::numbers::pi));
        point.size_ = static_cast<float>(std::min(16.0, point.size_ * scale));
        point.color_.alpha_ *= static_cast<float>(opacity);
        const auto velocity_x = point.velocity_x_ * aspect * scale;
        const auto velocity_y = point.velocity_y_ * scale;
        point.velocity_x_ = static_cast<float>((cosine * velocity_x - sine * velocity_y) / aspect);
        point.velocity_y_ = static_cast<float>(sine * velocity_x + cosine * velocity_y);
    }
    return std::make_shared<const particles::PointCloud>(std::move(points));
}
}  // namespace rhythm::runtime::detail
