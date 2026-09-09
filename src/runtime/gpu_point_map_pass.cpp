#include "gpu_point_map_pass.h"

#include <algorithm>
#include <cmath>
#include <numbers>
namespace rhythm::runtime::detail {
render::GpuPointHandle GpuPointMapPass::Evaluate(const graph::Instruction& instruction,
                                                 std::span<const NodeOutput> outputs,
                                                 render::Renderer& renderer) {
    const auto& input = outputs[instruction.inputs_.at(0).value()];
    if (input.gpu_sampling_) throw std::invalid_argument("graph.gpu_map_sample");
    if (!input.gpu_point_capacity_ || !renderer.IsValid(input.gpu_points_))
        throw std::invalid_argument("runtime.gpu_point_mapping_source");
    const auto control = [&](std::size_t port, std::string_view key, double fallback,
                             double minimum, double maximum) {
        const auto value = instruction.inputs_.at(port)
                                   ? outputs[*instruction.inputs_[port]].scalar_
                                   : graph::Scalar(instruction.node_, key, fallback);
        return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
    };
    const auto scale = control(1, "scale", 1, 0, 8);
    const auto angle = control(2, "rotation", 0, -36000, 36000) * std::numbers::pi / 180;
    const auto x = control(3, "translate_x", 0, -4, 4);
    const auto y = control(4, "translate_y", 0, -4, 4);
    const auto c = scale * std::cos(angle);
    const auto s = scale * std::sin(angle);
    render::GpuPointMapping mapping;
    mapping.transform_ = {float(c),
                          float(s),
                          0,
                          0,
                          float(-s),
                          float(c),
                          0,
                          0,
                          0,
                          0,
                          1,
                          0,
                          float(.5 - .5 * c + .5 * s + x),
                          float(.5 - .5 * s - .5 * c + y),
                          0,
                          1};
    const auto color = graph::ColorValue(instruction.node_, "color_a", {1, 1, 1, 1});
    mapping.color_ = {float(color.r_), float(color.g_), float(color.b_),
                      float(color.a_ * control(6, "opacity", 1, 0, 1))};
    mapping.size_ = float(control(5, "point_size_scale", 1, 0, 16));
    if (capacity_ != input.gpu_point_capacity_ || !renderer.IsValid(points_.Handle())) {
        points_ = {};
        points_ = renderer.CreateGpuPoints(input.gpu_point_capacity_);
        capacity_ = input.gpu_point_capacity_;
    }
    renderer.MapGpuPoints(input.gpu_points_, points_.Handle(), mapping);
    return points_.Handle();
}
}  // namespace rhythm::runtime::detail
