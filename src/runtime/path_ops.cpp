#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "scene_ops.h"

namespace rhythm::runtime::detail {
void EvaluatePath(const graph::Instruction& instruction, std::span<const NodeOutput> outputs,
                  NodeOutput& output) {
    const auto& node = instruction.node_;
    const auto scalar = [&](std::string_view key, double fallback) {
        return graph::Scalar(node, key, fallback);
    };
    const auto input = [&](std::size_t port) -> const NodeOutput& {
        if (port >= instruction.inputs_.size() || !instruction.inputs_[port] ||
            *instruction.inputs_[port] >= outputs.size())
            throw std::invalid_argument("runtime.path_input");
        return outputs[*instruction.inputs_[port]];
    };
    const auto control = [&](std::size_t port, std::string_view key, double fallback,
                             double minimum, double maximum) {
        const auto value =
                instruction.inputs_.at(port) ? input(port).scalar_ : scalar(key, fallback);
        return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
    };
    using graph::Operation;
    if (instruction.operation_ == Operation::kGeometryTube) {
        if (!input(0).path_) throw std::invalid_argument("runtime.path_input");
        auto model = scene::Tube(*input(0).path_, control(1, "tube_radius", 0.04, 0.0001, 10),
                                 std::uint32_t(scalar("tube_sides", 12)));
        output.geometry_ = std::make_shared<const scene::Geometry>(scene::Geometry{
                node.id_, output.version_, std::make_shared<const scene::Model>(std::move(model))});
        return;
    }
    const auto samples = std::uint32_t(scalar("path_samples", 192));
    scene::Path path;
    if (instruction.operation_ == Operation::kPathHelix) {
        path = scene::Helix(
                samples, control(0, "path_radius", 1.2, 0.001, 100),
                control(1, "path_height", 3, -100, 100), control(2, "path_turns", 3, 0.01, 32),
                control(3, "path_phase", 0, -36000, 36000), scalar("path_closed", 0) != 0);
    } else if (instruction.operation_ == Operation::kPathResample) {
        if (!input(0).path_) throw std::invalid_argument("runtime.path_input");
        path = scene::Resample(*input(0).path_, samples);
    } else if (instruction.operation_ == Operation::kPathFromPoints) {
        if (!input(0).points_) throw std::invalid_argument("runtime.path_input");
        const auto& points = *input(0).points_;
        const auto count = std::min(points.size(), std::size_t(samples));
        const auto scale = scalar("path_span", 4);
        path.closed_ = scalar("path_closed", 0) != 0;
        for (std::size_t i = 0; i < count; ++i) {
            const auto index = count < 2 ? 0 : i * (points.size() - 1) / (count - 1);
            const auto& point = points[index];
            const scene::Vector3 position{(point.x_ - 0.5) * scale, (0.5 - point.y_) * scale, 0};
            if (!path.points_.empty() && std::hypot(position.x_ - path.points_.back().x_,
                                                    position.y_ - path.points_.back().y_) < 1e-8)
                continue;
            path.points_.push_back(position);
        }
        if (path.closed_ && path.points_.size() > 1 &&
            std::hypot(path.points_.front().x_ - path.points_.back().x_,
                       path.points_.front().y_ - path.points_.back().y_) < 1e-8)
            path.points_.pop_back();
        if (path.points_.size() < (path.closed_ ? 3u : 2u)) path.points_.clear();
    } else
        throw std::invalid_argument("runtime.path_operation");
    scene::Validate(path);
    output.path_ = std::make_shared<const scene::Path>(std::move(path));
}
scene::Scene PreviewPath(const NodeOutput& output) {
    auto path = *output.path_;
    if (!path.points_.empty()) {
        auto low = path.points_.front(), high = low;
        for (const auto& point : path.points_) {
            low = {std::min(low.x_, point.x_), std::min(low.y_, point.y_),
                   std::min(low.z_, point.z_)};
            high = {std::max(high.x_, point.x_), std::max(high.y_, point.y_),
                    std::max(high.z_, point.z_)};
        }
        const auto span = std::max({high.x_ - low.x_, high.y_ - low.y_, high.z_ - low.z_, 0.001});
        for (auto& point : path.points_)
            point = {(point.x_ - (low.x_ + high.x_) * 0.5) * 1.6 / span,
                     (point.y_ - (low.y_ + high.y_) * 0.5) * 1.6 / span,
                     (point.z_ - (low.z_ + high.z_) * 0.5) * 1.6 / span};
    }
    auto model = scene::Tube(path, 0.012, 6);
    model.materials_[0].base_color_ = {0.05f, 0.8f, 1, 1};
    scene::Scene scene;
    scene.instances_.push_back({std::make_shared<const scene::Geometry>(
            scene::Geometry{output.node_, output.version_,
                            std::make_shared<const scene::Model>(std::move(model))})});
    return scene;
}
}  // namespace rhythm::runtime::detail
