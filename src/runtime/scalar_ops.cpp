#include "scalar_ops.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace rhythm::runtime::detail {
namespace {
constexpr double kScalarLimit = 1.0e12;
double Bounded(double value) {
    return std::isnan(value) ? 0 : std::clamp(value, -kScalarLimit, kScalarLimit);
}
double Wrap(double value, double duration) {
    const auto phase = std::fmod(value, duration);
    return phase < 0 ? phase + duration : phase;
}
double Noise(std::uint32_t index, std::uint32_t seed) {
    // Fixed-width avalanche and explicit normalization give reproducible lattice
    // samples across compilers/platforms; no global random state is involved.
    auto value = index ^ (seed + 0x9e3779b9u);
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value / 4294967295.0;
}
}  // namespace
std::optional<double> ExternalScalar(const graph::Instruction& instruction,
                                     const FrameContext& frame) {
    const auto& node = instruction.node_;
    switch (instruction.operation_) {
        case graph::Operation::kAudioFeature: {
            if (!frame.external_.audio_ || !frame.external_.audio_->valid_) return 0.0;
            const auto& audio = *frame.external_.audio_;
            switch (static_cast<int>(graph::Scalar(node, "audio_feature", 1))) {
                case 0:
                    return audio.rms_;
                case 1:
                    return audio.loudness_;
                case 2:
                    return audio.onset_strength_;
                case 3:
                    return audio.bpm_;
                case 4:
                    return audio.bpm_confidence_;
                case 5:
                    return audio.spectral_centroid_hz_;
                case 6:
                    return 1.0;
                default:
                    return 0.0;
            }
        }
        case graph::Operation::kAudioBand: {
            if (!frame.external_.audio_ || !frame.external_.audio_->valid_) return 0.0;
            const auto& audio = *frame.external_.audio_;
            const auto channel = graph::Scalar(node, "audio_channel", 0);
            const auto& bands = channel == 1   ? audio.left_bands_
                                : channel == 2 ? audio.right_bands_
                                               : audio.mono_bands_;
            return bands.at(static_cast<std::size_t>(graph::Scalar(node, "audio_band", 20)));
        }
        case graph::Operation::kSessionTime:
            return graph::Scalar(node, "session_value", 0) == 0
                           ? frame.external_.session_seconds_.value_or(frame.seconds_)
                           : (frame.external_.session_seconds_ ? 1.0 : 0.0);
        case graph::Operation::kParticipantRole:
            return graph::Scalar(node, "role_value", 0) == 0 ? frame.external_.participant_.index_
                                                             : frame.external_.participant_.group_;
        case graph::Operation::kSharedControl:
            return frame.external_.participant_.controls_.at(
                    static_cast<std::size_t>(graph::Scalar(node, "channel", 0)));
        default:
            return std::nullopt;
    }
}
double EvaluateScalar(const graph::Instruction& instruction, std::span<const NodeOutput> outputs,
                      double seconds) {
    const auto& node = instruction.node_;
    const auto input = [&](std::size_t port, std::string_view fallback, double default_value) {
        const auto slot = instruction.inputs_.at(port);
        return slot ? outputs[*slot].scalar_ : graph::Scalar(node, fallback, default_value);
    };
    switch (instruction.operation_) {
        case graph::Operation::kTime:
            return seconds;
        case graph::Operation::kSample:
            return input(0, "", 0);
        case graph::Operation::kConstant:
            return graph::Scalar(node, "value", 1);
        case graph::Operation::kExpression: {
            static const parameters::Expression default_expression;
            const auto found = node.properties_.find("expression");
            const auto& expression = found == node.properties_.end()
                                             ? default_expression
                                             : std::get<parameters::Expression>(found->second);
            return expression.Evaluate(
                    {input(0, "a", 0), input(1, "b", 0), input(2, "c", 0), input(3, "time", 0)});
        }
        case graph::Operation::kMap: {
            const auto minimum = graph::Scalar(node, "input_min", 0);
            const auto range = graph::Scalar(node, "input_max", 1) - minimum;
            auto unit = range == 0 ? 0 : (Bounded(input(0, "", 0)) - minimum) / range;
            if (graph::Scalar(node, "map_mode", 0) == 0) unit = std::clamp(unit, 0.0, 1.0);
            const auto start = graph::Scalar(node, "output_min", 0);
            if (unit == 0) return start;
            const auto distance = graph::Scalar(node, "output_max", 1) - start;
            return distance == 0 ? start : Bounded(start + distance * unit);
        }
        case graph::Operation::kCompare: {
            const auto a = Bounded(input(0, "a", 0));
            const auto b = Bounded(input(1, "b", 0.5));
            const auto epsilon = graph::Scalar(node, "epsilon", 0.000001);
            switch (static_cast<int>(graph::Scalar(node, "compare_mode", 0))) {
                case 0:
                    return a > b;
                case 1:
                    return a < b;
                case 2:
                    return std::abs(a - b) <= epsilon;
                case 3:
                    return std::abs(a - b) > epsilon;
                case 4:
                    return a >= b;
                case 5:
                    return a <= b;
                default:
                    throw std::invalid_argument("runtime.compare_mode");
            }
        }
        case graph::Operation::kSelect:
            return Bounded(input(0, "", 0) > 0 ? input(2, "b", 1) : input(1, "a", 0));
        case graph::Operation::kNoise: {
            const auto phase = Wrap(Bounded(input(0, "", 0)) * graph::Scalar(node, "frequency", 1),
                                    4294967296.0);
            const auto index = static_cast<std::uint32_t>(std::floor(phase));
            const auto seed = static_cast<std::uint32_t>(graph::Scalar(node, "seed", 0));
            const auto first = Noise(index, seed);
            const auto mode = graph::Scalar(node, "noise_mode", 2);
            if (mode == 0) return first;
            auto fraction = phase - std::floor(phase);
            if (mode == 2) fraction = fraction * fraction * (3 - 2 * fraction);
            return first + (Noise(index + 1, seed) - first) * fraction;
        }
        case graph::Operation::kCurve: {
            static const parameters::Curve kDefaultCurve;
            const auto found = node.properties_.find("curve");
            const auto& curve = found == node.properties_.end()
                                        ? kDefaultCurve
                                        : std::get<parameters::Curve>(found->second);
            return curve.Evaluate(input(0, "", 0));
        }
        case graph::Operation::kOscillator: {
            const auto frequency = graph::Scalar(node, "frequency", 0.25);
            const auto phase = Wrap(input(0, "", 0), 1 / frequency) * frequency;
            return 0.5 + 0.5 * std::sin(phase * 2 * std::numbers::pi);
        }
        case graph::Operation::kMath: {
            const auto a = Bounded(input(0, "a", 0));
            const auto b = Bounded(input(1, "b", 1));
            switch (static_cast<int>(graph::Scalar(node, "math_mode", 0))) {
                case 0:
                    return Bounded(a + b);
                case 1:
                    return Bounded(a - b);
                case 2:
                    return Bounded(a * b);
                case 3:
                    return b == 0 ? 0 : Bounded(a / b);
                case 4:
                    return std::min(a, b);
                case 5:
                    return std::max(a, b);
                default:
                    throw std::invalid_argument("runtime.math_mode");
            }
        }
        case graph::Operation::kLocalTime: {
            const auto time = Bounded(input(0, "", 0)) * graph::Scalar(node, "speed", 1) +
                              graph::Scalar(node, "offset", 0);
            const auto duration = graph::Scalar(node, "duration", 4);
            switch (static_cast<int>(graph::Scalar(node, "time_mode", 0))) {
                case 0:
                    return std::max(0.0, time);
                case 1:
                    return Wrap(time, duration);
                case 2:
                    return duration - std::abs(Wrap(time, 2 * duration) - duration);
                default:
                    throw std::invalid_argument("runtime.time_mode");
            }
        }
        default:
            throw std::invalid_argument("runtime.scalar_operator");
    }
}
}  // namespace rhythm::runtime::detail
