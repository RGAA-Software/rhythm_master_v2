#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "audio_spectrum.h"
#include "scene_ops.h"

namespace rhythm::runtime::detail {
std::shared_ptr<const scene::Scene> PointInstances(const graph::Instruction& instruction,
                                                   std::span<const NodeOutput> outputs,
                                                   const ExternalInputs& external) {
    const auto input = [&](std::size_t port) -> const NodeOutput& {
        const auto slot = instruction.inputs_.at(port);
        if (!slot || *slot >= outputs.size()) throw std::invalid_argument("runtime.instance_input");
        return outputs[*slot];
    };
    const auto& node = instruction.node_;
    const auto control = [&](std::size_t port, std::string_view key, double fallback,
                             double minimum) {
        const auto value = instruction.inputs_.at(port) ? input(port).scalar_
                                                        : graph::Scalar(node, key, fallback);
        return std::isfinite(value) ? std::clamp(value, minimum, 100.0) : fallback;
    };
    const auto& geometry = input(0).geometry_;
    const auto& points = input(1).points_;
    const auto limit = graph::Scalar(node, "instance_limit", 1024);
    if (!geometry || !points || limit < 1 || limit > graph::kMaximumSceneInstances ||
        !std::isfinite(limit) || points->size() > limit)
        throw std::length_error("runtime.instance_capacity");
    const auto span = graph::Scalar(node, "instance_span", 10);
    const auto scale = control(3, "scale", 1, 0.001);
    const auto height = control(4, "height", 1, 0.001);
    const auto gain = control(5, "audio_gain", 12, 0);
    const auto bands = SpectrumBands(node, external);
    const bool point_color = graph::Scalar(node, "instance_point_color", 0) != 0;
    const auto material = instruction.inputs_.at(2) ? input(2).material_ : std::nullopt;
    scene::Scene result;
    result.instances_.reserve(points->size());
    for (const auto& point : *points) {
        if (!std::isfinite(point.x_) || !std::isfinite(point.y_) ||
            !std::isfinite(point.rotation_) || !std::isfinite(point.size_) || point.size_ < 0)
            throw std::invalid_argument("runtime.instance_point");
        if (point.size_ == 0) continue;
        const auto band_index =
                bands.empty() ? 0
                              : static_cast<std::size_t>(std::clamp(point.x_, 0.0f, 1.0f) *
                                                         static_cast<float>(bands.size() - 1));
        const auto amplitude =
                bands.empty() || !std::isfinite(bands[band_index])
                        ? 0.0
                        : std::clamp(static_cast<double>(bands[band_index]), 0.0, 1.0);
        const auto width = std::clamp(point.size_ * span * scale, 0.001, 100.0);
        const auto vertical = std::clamp(width * (height + amplitude * gain), 0.001, 100.0);
        scene::Instance instance;
        instance.geometry_ = geometry;
        instance.transform_ = scene::Compose(
                {(point.x_ - 0.5) * span, vertical * 0.5, (point.y_ - 0.5) * span},
                {0, std::sin(point.rotation_ * 0.5), 0, std::cos(point.rotation_ * 0.5)},
                {width, vertical, width});
        instance.material_ = material;
        if (point_color) {
            auto colored = material.value_or(scene::Material{});
            auto& color = colored.base_color_;
            color.red_ *= point.color_.red_;
            color.green_ *= point.color_.green_;
            color.blue_ *= point.color_.blue_;
            color.alpha_ *= point.color_.alpha_;
            instance.material_ = colored;
        }
        result.instances_.push_back(std::move(instance));
    }
    return std::make_shared<const scene::Scene>(std::move(result));
}
}  // namespace rhythm::runtime::detail
