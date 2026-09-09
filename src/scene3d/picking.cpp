#include "rhythm/scene/picking.h"

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/intersect.hpp>
#include <limits>
#include <stdexcept>

namespace rhythm::scene {
namespace {
glm::dvec3 Native(Vector3 value) { return {value.x_, value.y_, value.z_}; }
glm::dvec3 Native(const Vertex& value) { return {value.x_, value.y_, value.z_}; }
Vector3 Value(glm::dvec3 value) { return {value.x, value.y, value.z}; }
bool Finite(glm::dvec3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}
void Consume(std::size_t& remaining, std::size_t count) {
    if (count > remaining) throw std::length_error("scene_pick.budget");
    remaining -= count;
}
struct Bounds {
    glm::dvec3 minimum_{std::numeric_limits<double>::infinity()};
    glm::dvec3 maximum_{-std::numeric_limits<double>::infinity()};
};
bool Intersects(const Bounds& bounds, glm::dvec3 origin, glm::dvec3 direction, double nearest) {
    double first = 0;
    double last = nearest;
    for (int axis = 0; axis < 3; ++axis) {
        if (direction[axis] == 0) {
            if (origin[axis] < bounds.minimum_[axis] || origin[axis] > bounds.maximum_[axis])
                return false;
        } else {
            auto low = (bounds.minimum_[axis] - origin[axis]) / direction[axis];
            auto high = (bounds.maximum_[axis] - origin[axis]) / direction[axis];
            if (low > high) std::swap(low, high);
            first = std::max(first, low);
            last = std::min(last, high);
            if (last < first) return false;
        }
    }
    return true;
}
struct Prepared {
    std::map<NodeId, WorldNode> worlds_{};
    std::vector<Bounds> bounds_{};
};
Prepared Prepare(const Geometry& geometry, PickBudget& budget) {
    if (!geometry.model_) throw std::invalid_argument("scene_pick.geometry");
    const auto& model = *geometry.model_;
    if (!geometry.deformations_.empty() || !model.skins_.empty())
        throw std::invalid_argument("scene_pick.deformation");
    Consume(budget.nodes_, model.nodes_.size());
    Prepared result;
    result.worlds_ = WorldTransforms(model, geometry.pose_ ? *geometry.pose_ : model.rest_pose_);
    Consume(budget.nodes_, model.meshes_.size());
    for (const auto& mesh : model.meshes_) {
        if (!mesh.morphs_.empty() || !mesh.skin_.empty())
            throw std::invalid_argument("scene_pick.deformation");
        Consume(budget.vertices_, mesh.vertices_.size());
        Bounds bounds;
        for (const auto& vertex : mesh.vertices_) {
            const auto position = Native(vertex);
            if (!Finite(position)) throw std::invalid_argument("scene_pick.geometry");
            bounds.minimum_ = glm::min(bounds.minimum_, position);
            bounds.maximum_ = glm::max(bounds.maximum_, position);
        }
        result.bounds_.push_back(bounds);
    }
    return result;
}
}  // namespace
std::optional<PickRay> CameraRay(const Camera& camera, double aspect, double x, double y) {
    if (!std::isfinite(x) || !std::isfinite(y) || x < 0 || x > 1 || y < 0 || y > 1)
        return std::nullopt;
    try {
        const auto matrix = Multiply(Projection(camera, aspect), View(camera));
        const auto inverse = glm::inverse(glm::make_mat4(matrix.values_.data()));
        const auto near = inverse * glm::dvec4(2 * x - 1, 1 - 2 * y, -1, 1);
        const auto far = inverse * glm::dvec4(2 * x - 1, 1 - 2 * y, 1, 1);
        const auto start = glm::dvec3(near) / near.w;
        const auto end = glm::dvec3(far) / far.w;
        if (!Finite(start) || !Finite(end) || glm::length(end - start) < 1e-12) return std::nullopt;
        return PickRay{Value(start), Value(end)};
    } catch (const std::exception&) {
        return std::nullopt;
    }
}
PickResult PickScene(const Scene& scene, const PickRay& ray, PickBudget budget) {
    PickResult result;
    try {
        const auto start = Native(ray.near_);
        const auto direction = Native(ray.far_) - start;
        if (!Finite(start) || !Finite(direction) || glm::length(direction) < 1e-12)
            throw std::invalid_argument("scene_pick.ray");
        Consume(budget.instances_, scene.instances_.size());
        // Shared ownership keys are local to this synchronous call. Geometry IDs
        // can be reused by another plan without retaining a stale picking cache.
        std::map<std::shared_ptr<const Geometry>, Prepared,
                 std::owner_less<std::shared_ptr<const Geometry>>>
                prepared;
        double nearest = 1;
        for (std::size_t index = 0; index < scene.instances_.size(); ++index) {
            const auto& instance = scene.instances_[index];
            if (!instance.geometry_) throw std::invalid_argument("scene_pick.geometry");
            if (!prepared.contains(instance.geometry_))
                prepared.emplace(instance.geometry_, Prepare(*instance.geometry_, budget));
            const auto& cached = prepared.at(instance.geometry_);
            const auto& model = *instance.geometry_->model_;
            Consume(budget.nodes_, model.nodes_.size());
            for (const auto& node : model.nodes_) {
                const auto& world = cached.worlds_.at(node.id_);
                if (!world.visible_) continue;
                const auto transform = Multiply(instance.transform_, world.transform_);
                const auto inverse = InverseAffine(transform);
                const auto origin = Native(TransformPoint(inverse, ray.near_));
                const auto local_direction = Native(TransformPoint(inverse, ray.far_)) - origin;
                Consume(budget.nodes_, node.meshes_.size());
                for (const auto mesh_index : node.meshes_) {
                    const auto& mesh = model.meshes_.at(mesh_index);
                    if (!Intersects(cached.bounds_.at(mesh_index), origin, local_direction,
                                    nearest))
                        continue;
                    const auto material =
                            instance.material_.value_or(model.materials_.at(mesh.material_));
                    if (material.base_color_.alpha_ <= 0) continue;
                    if (mesh.indices_.size() % 3)
                        throw std::invalid_argument("scene_pick.geometry");
                    Consume(budget.triangles_, mesh.indices_.size() / 3);
                    for (std::size_t triangle = 0; triangle < mesh.indices_.size(); triangle += 3) {
                        ++result.triangles_;
                        const auto a = Native(mesh.vertices_.at(mesh.indices_[triangle]));
                        const auto b = Native(mesh.vertices_.at(mesh.indices_[triangle + 1]));
                        const auto c = Native(mesh.vertices_.at(mesh.indices_[triangle + 2]));
                        if (!material.double_sided_ &&
                            glm::dot(glm::cross(b - a, c - a), local_direction) >= 0)
                            continue;
                        glm::dvec2 barycentric;
                        double fraction = 0;
                        if (!glm::intersectRayTriangle(origin, local_direction, a, b, c,
                                                       barycentric, fraction) ||
                            !std::isfinite(fraction) || fraction < 0 || fraction > nearest)
                            continue;
                        if (result.hit_ && fraction == nearest) continue;
                        nearest = fraction;
                        result.hit_ = PickHit{instance.origin_, node.id_, index,
                                              Value(start + fraction * direction), fraction};
                    }
                }
            }
        }
    } catch (const std::exception& error) {
        result.hit_.reset();
        result.error_ = error.what();
    }
    return result;
}
}  // namespace rhythm::scene
