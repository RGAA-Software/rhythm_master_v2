#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/vector_angle.hpp>
#include <numbers>
#include <stdexcept>

#include "rhythm/scene/path.h"

namespace rhythm::scene {
namespace {
glm::dvec3 Native(Vector3 value) { return {value.x_, value.y_, value.z_}; }
Vertex MakeVertex(glm::dvec3 point, glm::dvec3 normal, float u, float v) {
    return {float(point.x),
            float(point.y),
            float(point.z),
            float(normal.x),
            float(normal.y),
            float(normal.z),
            u,
            v};
}
}  // namespace
Model Tube(const Path& path, double radius, std::uint32_t sides) {
    Validate(path);
    if (!std::isfinite(radius) || radius < 0.0001 || radius > 10 || sides < 3 || sides > 32)
        throw std::invalid_argument("path.tube_parameters");
    if (path.points_.empty()) return Model{{}, {Material{}}, {{1, {}, {}, {}, true, "Tube"}}};
    const auto count = path.points_.size();
    std::vector<glm::dvec3> tangents(count), normals(count);
    for (std::size_t i = 0; i < count; ++i) {
        const auto previous = i ? i - 1 : path.closed_ ? count - 1 : 0;
        const auto next = i + 1 < count ? i + 1 : path.closed_ ? 0 : count - 1;
        const auto delta = Native(path.points_[next]) - Native(path.points_[previous]);
        if (glm::length(delta) < 1e-8) throw std::invalid_argument("path.cusp");
        tangents[i] = glm::normalize(delta);
    }
    const glm::dvec3 up = std::abs(tangents[0].y) < 0.9 ? glm::dvec3(0, 1, 0) : glm::dvec3(1, 0, 0);
    normals[0] = glm::normalize(glm::cross(up, tangents[0]));
    for (std::size_t i = 1; i < count; ++i) {
        const auto transported = glm::rotation(tangents[i - 1], tangents[i]) * normals[i - 1];
        normals[i] = glm::normalize(transported - tangents[i] * glm::dot(transported, tangents[i]));
    }
    double correction = 0;
    if (path.closed_) {
        const auto closing = glm::rotation(tangents.back(), tangents.front()) * normals.back();
        correction = glm::orientedAngle(glm::normalize(closing), normals.front(), tangents.front());
    }
    Mesh mesh;
    const auto rings = count + (path.closed_ ? 1 : 0);
    const auto stride = sides + 1;
    for (std::size_t i = 0; i < rings; ++i) {
        const auto source = i % count;
        const auto normal = i == count ? normals[0]
                                       : glm::angleAxis(correction * double(i) / double(count),
                                                        tangents[source]) *
                                                 normals[source];
        const auto bitangent = glm::cross(tangents[source], normal);
        for (std::uint32_t side = 0; side <= sides; ++side) {
            const auto angle = 2 * std::numbers::pi * (side == sides ? 0 : side) / sides;
            const auto radial = normal * std::cos(angle) + bitangent * std::sin(angle);
            mesh.vertices_.push_back(MakeVertex(Native(path.points_[source]) + radial * radius,
                                                radial, float(side) / float(sides),
                                                float(i) / float(rings - 1)));
        }
    }
    // TiXL ExtrudeCurves rail/profile grid with cyclically equivalent CCW triangles.
    for (std::uint32_t i = 0; i + 1 < rings; ++i)
        for (std::uint32_t side = 0; side < sides; ++side) {
            const auto a = i * stride + side, b = a + stride;
            mesh.indices_.insert(mesh.indices_.end(), {a, a + 1, b, a + 1, b + 1, b});
        }
    if (!path.closed_) {
        for (bool end : {false, true}) {
            const auto ring = end ? count - 1 : 0;
            const auto center = std::uint32_t(mesh.vertices_.size());
            const auto normal = tangents[ring] * (end ? 1.0 : -1.0);
            mesh.vertices_.push_back(MakeVertex(Native(path.points_[ring]), normal, 0.5f, 0.5f));
            for (std::uint32_t side = 0; side <= sides; ++side) {
                const auto& point = mesh.vertices_[ring * stride + side];
                const auto angle = 2 * std::numbers::pi * side / sides;
                mesh.vertices_.push_back(MakeVertex({point.x_, point.y_, point.z_}, normal,
                                                    float(0.5 + 0.5 * std::cos(angle)),
                                                    float(0.5 + 0.5 * std::sin(angle))));
            }
            for (std::uint32_t side = 0; side < sides; ++side) {
                const auto a = center + side + 1, b = a + 1;
                if (end)
                    mesh.indices_.insert(mesh.indices_.end(), {center, a, b});
                else
                    mesh.indices_.insert(mesh.indices_.end(), {center, b, a});
            }
        }
    }
    Model result;
    result.meshes_.push_back(std::move(mesh));
    result.materials_.push_back({});
    result.nodes_.push_back({1, {}, {}, {0}, true, "Tube"});
    Validate(result);
    return result;
}
}  // namespace rhythm::scene
