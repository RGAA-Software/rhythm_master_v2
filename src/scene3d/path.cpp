#include "rhythm/scene/path.h"

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtx/spline.hpp>
#include <numbers>
#include <stdexcept>

namespace rhythm::scene {
namespace {
glm::dvec3 Native(Vector3 value) { return {value.x_, value.y_, value.z_}; }
Vector3 Value(glm::dvec3 value) { return {value.x, value.y, value.z}; }
}  // namespace
void Validate(const Path& path) {
    if (path.points_.empty()) return;
    if (path.points_.size() < (path.closed_ ? 3u : 2u) || path.points_.size() > kMaximumPathPoints)
        throw std::invalid_argument("path.point_count");
    for (std::size_t i = 0; i < path.points_.size(); ++i) {
        const auto point = Native(path.points_[i]);
        if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z) ||
            glm::any(glm::greaterThan(glm::abs(point), glm::dvec3(10000))))
            throw std::invalid_argument("path.position");
        if ((i || path.closed_) &&
            glm::length(point -
                        Native(path.points_[(i + path.points_.size() - 1) % path.points_.size()])) <
                    1e-8)
            throw std::invalid_argument("path.coincident_points");
    }
}
Path Helix(std::uint32_t points, double radius, double height, double turns, double phase,
           bool closed) {
    if (points < 3 || points > kMaximumPathPoints || !std::isfinite(radius) || radius < 0.001 ||
        radius > 100 || !std::isfinite(height) || std::abs(height) > 100 || !std::isfinite(turns) ||
        std::abs(turns) < 0.01 || std::abs(turns) > 32 || !std::isfinite(phase) ||
        std::abs(phase) > 36000)
        throw std::invalid_argument("path.helix_parameters");
    Path result;
    result.closed_ = closed;
    result.points_.reserve(points);
    for (std::uint32_t i = 0; i < points; ++i) {
        const auto t = double(i) / (closed ? points : points - 1);
        const auto angle = (turns * t * 360 + phase) * std::numbers::pi / 180;
        result.points_.push_back(
                {radius * std::cos(angle), (t - 0.5) * height, radius * std::sin(angle)});
    }
    Validate(result);
    return result;
}
Path Resample(const Path& path, std::uint32_t points) {
    Validate(path);
    if (points < (path.closed_ ? 3u : 2u) || points > kMaximumPathPoints)
        throw std::invalid_argument("path.point_count");
    if (path.points_.empty()) return path;
    const auto count = std::int32_t(path.points_.size());
    const auto source = [&](std::int32_t index) {
        const auto bounded =
                path.closed_ ? (index % count + count) % count : std::clamp(index, 0, count - 1);
        return Native(path.points_[std::size_t(bounded)]);
    };
    Path result;
    result.closed_ = path.closed_;
    result.points_.reserve(points);
    for (std::uint32_t i = 0; i < points; ++i) {
        const auto parameter = double(i) * (path.closed_ ? count : count - 1) /
                               (path.closed_ ? points : points - 1);
        const auto index = std::min(std::int32_t(parameter), count - 1);
        result.points_.push_back(
                Value(glm::catmullRom(source(index - 1), source(index), source(index + 1),
                                      source(index + 2), parameter - index)));
    }
    Validate(result);
    return result;
}
}  // namespace rhythm::scene
