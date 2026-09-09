#include "spectrum_points.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

#include "audio_spectrum.h"

namespace rhythm::runtime::detail {
std::shared_ptr<const particles::PointCloud> SpectrumPoints(const graph::Instruction& instruction,
                                                            std::span<const NodeOutput> outputs,
                                                            const ExternalInputs& external) {
    const auto& node = instruction.node_;
    const auto scalar = [&](std::string_view key, double fallback) {
        return graph::Scalar(node, key, fallback);
    };
    const auto count_value = scalar("point_count", 128);
    if (!std::isfinite(count_value) || count_value < 3 || count_value > 512 ||
        std::floor(count_value) != count_value)
        throw std::length_error("graph.points_budget");
    const auto count = static_cast<std::uint32_t>(count_value);
    auto gain = scalar("spectrum_gain", 2);
    if (!instruction.inputs_.empty() && instruction.inputs_[0])
        gain = outputs[*instruction.inputs_[0]].scalar_;
    gain = std::isfinite(gain) ? std::clamp(gain, 0.0, 100.0) : 0;
    const auto bands = SpectrumBands(node, external);
    const auto first = scalar("band_first", 0), last = scalar("band_last", 62);
    const auto low_color = graph::ColorValue(node, "color_a", {.1, .8, .9, 1});
    const auto high_color = graph::ColorValue(node, "color_b", {1, .3, .6, 1});
    const auto radial = scalar("spectrum_layout", 1) == 1;
    auto points = std::make_shared<particles::PointCloud>();
    points->reserve(count);
    for (std::uint32_t index = 0; index < count; ++index) {
        const auto position = static_cast<double>(index) / (count - 1);
        const auto sample = std::lerp(first, last, position);
        const auto amount = std::clamp(SpectrumSample(bands, sample) * gain, 0.0, 1.0);
        const auto height = amount * scalar("spectrum_height", .25);
        const auto angle = 2 * std::numbers::pi * index / count - std::numbers::pi / 2;
        const auto radius = scalar("spectrum_radius", .2) + height;
        particles::Point point;
        point.id_ = index + 1;
        point.x_ = static_cast<float>(scalar("center_x", .5) +
                                      (radial ? std::cos(angle) * radius : position - .5));
        point.y_ = static_cast<float>(scalar("center_y", .5) +
                                      (radial ? std::sin(angle) * radius : -height));
        point.size_ = static_cast<float>(scalar("point_size", .008));
        const auto color = [&](double low, double high) {
            return static_cast<float>(std::lerp(low, high, amount));
        };
        point.color_ = {color(low_color.r_, high_color.r_), color(low_color.g_, high_color.g_),
                        color(low_color.b_, high_color.b_), color(low_color.a_, high_color.a_)};
        points->push_back(point);
    }
    return points;
}
}  // namespace rhythm::runtime::detail
