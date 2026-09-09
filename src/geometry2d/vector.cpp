#include "rhythm/geometry2d/vector.h"

#ifdef _MSC_VER
#pragma warning(push)
// Clipper2's exception-enabled CheckPrecisionRange retains an unreachable
// clamp after DoError throws. Suppress only inside the unmodified upstream API.
#pragma warning(disable : 4702)
#endif
#include <clipper2/clipper.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <mapbox/earcut.hpp>
#include <stdexcept>

namespace rhythm::geometry2d {
namespace {
namespace clip = Clipper2Lib;
constexpr std::size_t kMaximumIndices = 49152;
void Validate(std::span<const Contour> contours, bool require_closed) {
    if (contours.size() > 64) throw std::length_error("vector.contour_count");
    std::size_t points = 0;
    for (const auto& contour : contours) {
        if (contour.points_.size() > kMaximumVectorPoints - points)
            throw std::length_error("vector.point_count");
        points += contour.points_.size();
        if (require_closed && !contour.closed_) throw std::invalid_argument("vector.open_fill");
        for (const auto point : contour.points_)
            if (!std::isfinite(point.x_) || !std::isfinite(point.y_) ||
                std::abs(point.x_) > 10000 || std::abs(point.y_) > 10000)
                throw std::invalid_argument("vector.coordinate");
    }
}
clip::PathD Convert(const Contour& contour) {
    clip::PathD result;
    result.reserve(contour.points_.size());
    for (const auto point : contour.points_) result.emplace_back(point.x_, point.y_);
    return result;
}
// Clipper's tree owns every child. Borrowed references remain private to this
// synchronous adapter; project callers receive only value vertices/indices.
void Append(const clip::PolyPathD& polygon, VectorMesh& mesh, std::size_t depth) {
    if (depth > 64) throw std::length_error("vector.nesting");
    if (!polygon.Polygon().empty() && !polygon.IsHole()) {
        std::vector<std::vector<std::array<double, 2>>> rings;
        std::size_t points = 0;
        const auto ring = [&](const clip::PathD& path) {
            if (path.size() > kMaximumVectorVertices - mesh.vertices_.size() - points)
                throw std::length_error("vector.vertex_count");
            points += path.size();
            std::vector<std::array<double, 2>> values;
            values.reserve(path.size());
            for (const auto& point : path) values.push_back({point.x, point.y});
            rings.push_back(std::move(values));
        };
        ring(polygon.Polygon());
        for (const auto& hole : polygon) ring(hole->Polygon());
        const auto indices = mapbox::earcut<std::uint32_t>(rings);
        if (indices.size() % 3 || indices.size() > kMaximumIndices - mesh.indices_.size())
            throw std::length_error("vector.index_count");
        const auto base = static_cast<std::uint32_t>(mesh.vertices_.size());
        for (const auto& values : rings)
            for (const auto point : values) mesh.vertices_.push_back({point[0], point[1]});
        for (const auto index : indices) {
            if (index >= points) throw std::runtime_error("vector.invalid_triangle");
            mesh.indices_.push_back(base + index);
        }
    }
    for (const auto& child : polygon) Append(*child, mesh, depth + 1);
}
VectorMesh Triangulate(const clip::PathsD& paths, clip::FillRule rule) {
    clip::ClipperD engine(6);
    engine.AddSubject(paths);
    clip::PolyTreeD tree;
    clip::PathsD open;
    if (!engine.Execute(clip::ClipType::Union, rule, tree, open))
        throw std::runtime_error("vector.clip_failed");
    VectorMesh mesh;
    Append(tree, mesh, 0);
    return mesh;
}
clip::JoinType Join(LineJoin join) {
    switch (join) {
        case LineJoin::kMiter:
            return clip::JoinType::Miter;
        case LineJoin::kBevel:
            return clip::JoinType::Bevel;
        case LineJoin::kRound:
            return clip::JoinType::Round;
    }
    throw std::invalid_argument("vector.join");
}
clip::EndType Cap(LineCap cap) {
    switch (cap) {
        case LineCap::kButt:
            return clip::EndType::Butt;
        case LineCap::kSquare:
            return clip::EndType::Square;
        case LineCap::kRound:
            return clip::EndType::Round;
    }
    throw std::invalid_argument("vector.cap");
}
}  // namespace
VectorMesh FillContours(std::span<const Contour> contours) {
    Validate(contours, true);
    clip::PathsD paths;
    for (const auto& contour : contours)
        if (contour.points_.size() >= 3) paths.push_back(Convert(contour));
    return Triangulate(paths, clip::FillRule::EvenOdd);
}
VectorMesh StrokeContours(std::span<const Contour> contours, const StrokeStyle& style) {
    Validate(contours, false);
    if (!std::isfinite(style.width_) || style.width_ < 0 || style.width_ > 100 ||
        !std::isfinite(style.miter_limit_) || style.miter_limit_ < 1 || style.miter_limit_ > 16 ||
        !std::isfinite(style.arc_tolerance_) || style.arc_tolerance_ < .001 ||
        style.arc_tolerance_ > 1)
        throw std::invalid_argument("vector.stroke_style");
    const auto join = Join(style.join_);
    const auto cap = Cap(style.cap_);
    if (style.width_ == 0) return {};
    clip::PathsD paths;
    std::size_t points = 0;
    for (const auto& contour : contours) {
        if (contour.points_.size() < 2) continue;
        auto expanded = clip::InflatePaths({Convert(contour)}, style.width_ / 2, join,
                                           contour.closed_ ? clip::EndType::Joined : cap,
                                           style.miter_limit_, 6, style.arc_tolerance_);
        for (auto& path : expanded) {
            if (path.size() > kMaximumVectorVertices - points)
                throw std::length_error("vector.vertex_count");
            points += path.size();
            paths.push_back(std::move(path));
        }
    }
    return Triangulate(paths, clip::FillRule::NonZero);
}
}  // namespace rhythm::geometry2d
