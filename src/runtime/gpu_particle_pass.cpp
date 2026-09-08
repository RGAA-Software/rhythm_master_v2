#include "gpu_particle_pass.h"

#include <algorithm>
#include <cmath>
namespace rhythm::runtime::detail {
namespace {
std::array<float, 4> Color(graph::Color c) {
    return {float(c.r_), float(c.g_), float(c.b_), float(c.a_)};
}
double Control(const graph::Instruction& instruction, std::span<const NodeOutput> outputs,
               std::size_t port, std::string_view key, double fallback, double maximum) {
    const double value = instruction.inputs_.at(port)
                                 ? outputs[*instruction.inputs_[port]].scalar_
                                 : graph::Scalar(instruction.node_, key, fallback);
    return std::isfinite(value) ? std::clamp(value, 0.0, maximum) : fallback;
}
render::GpuParticleStep Settings(const graph::Node& node) {
    const auto value = [&](std::string_view key, double fallback) {
        return float(graph::Scalar(node, key, fallback));
    };
    render::GpuParticleStep s;
    s.seed_ = static_cast<std::uint32_t>(graph::Scalar(node, "seed", 1));
    s.center_ = {value("center_x", 0.5), value("center_y", 0.5), 0};
    s.radius_ = value("emitter_radius", 0.3);
    s.speed_ = value("particle_speed", 0.08);
    s.drag_ = value("drag", 0.1);
    s.lifetime_ = value("lifetime", 4);
    s.size_ = value("point_size", 0.002);
    s.gravity_ = {value("gravity_x", 0), value("gravity_y", 0)};
    s.frequency_ = value("flow_frequency", 8);
    s.color_a_ = Color(graph::ColorValue(node, "color_a", {0.05, 0.6, 1, 0.2}));
    s.color_b_ = Color(graph::ColorValue(node, "color_b", {1, 0.1, 0.4, 0.2}));
    return s;
}
}  // namespace
render::GpuPointHandle GpuParticlePass::Evaluate(const graph::Instruction& instruction,
                                                 std::span<const NodeOutput> outputs,
                                                 FrameContext frame, render::Renderer& renderer) {
    constexpr double kStep = 1.0 / 60;
    constexpr double kMaximumDelta = 8 * kStep;
    const auto& node = instruction.node_;
    const auto capacity =
            static_cast<std::uint32_t>(graph::Scalar(node, "particle_capacity", 65536));
    if (capacity_ != capacity || !renderer.IsValid(points_.Handle())) {
        // Release before reallocating to avoid temporary double admission.
        points_ = {};
        points_ = renderer.CreateGpuPoints(capacity);
        capacity_ = capacity;
        configuration_.reset();
    }
    const double delta = last_seconds_ ? frame.seconds_ - *last_seconds_ : 0;
    const bool reset =
            !configuration_ || *configuration_ != node || delta < 0 || delta > kMaximumDelta + 1e-9;
    auto step = Settings(node);
    step.flow_ = float(Control(instruction, outputs, 2, "flow_strength", 0.1, 8));
    const auto evolution = graph::Scalar(node, "flow_evolution", 0.3);
    step.phase_ = float(std::fmod(frame.seconds_ * evolution, 4096.0));
    const double burst = Control(instruction, outputs, 1, "burst", 0, capacity_);
    const auto emit = [&](std::uint32_t count) {
        step.spawn_start_ = cursor_;
        step.spawn_count_ = count;
        step.sequence_ = sequence_;
        renderer.UpdateGpuParticles(points_.Handle(), step);
        cursor_ = (cursor_ + count) % capacity_;
        sequence_ = (sequence_ + 1) & 0xffffffU;
    };
    if (reset) {
        accumulator_ = 0;
        emission_fraction_ = 0;
        cursor_ = 0;
        sequence_ = 0;
        burst_high_ = false;
        step.reset_ = true;
        emit(static_cast<std::uint32_t>(capacity_ * graph::Scalar(node, "initial_fill", 1)));
        configuration_ = node;
    }
    if (frame.advance_state_) {
        step.reset_ = false;
        if (burst > 0 && !burst_high_) emit(static_cast<std::uint32_t>(burst));
        burst_high_ = burst > 0;
        if (!reset) accumulator_ += delta;
        const auto rate = graph::Scalar(node, "emission_rate", 16000) *
                          Control(instruction, outputs, 0, "emission", 1, 100);
        for (int i = 0; i < 8 && accumulator_ + 1e-9 >= kStep; ++i) {
            step.seconds_ = float(kStep);
            // Phase follows each simulation step, including multi-step export frames.
            step.phase_ =
                    float(std::fmod((frame.seconds_ - accumulator_ + kStep) * evolution, 4096.0));
            emission_fraction_ = std::min(double(capacity_), emission_fraction_ + rate * kStep);
            const auto count = static_cast<std::uint32_t>(emission_fraction_);
            emit(count);
            emission_fraction_ -= count;
            accumulator_ = std::max(0.0, accumulator_ - kStep);
        }
    }
    last_seconds_ = frame.seconds_;
    return points_.Handle();
}
}  // namespace rhythm::runtime::detail
