#include <cmath>
#include <iostream>
#include <numbers>
#include <stdexcept>

#include "rhythm/scene/camera.h"
#include "rhythm/scene/model.h"

namespace {
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void Run() {
    using namespace rhythm::scene;
    Camera camera;
    camera.near_ = 0.1;
    camera.far_ = 100;
    const auto view = View(camera);
    Require(TransformPoint(view, camera.eye_) == Vector3{} &&
                    TransformPoint(view, camera.target_).z_ == -3,
            "camera eye at origin and forward negative Z");
    for (const auto aspect : {16.0 / 9, 9.0 / 16, 1.0})
        for (const auto kind : {ProjectionKind::kPerspective, ProjectionKind::kOrthographic}) {
            camera.kind_ = kind;
            const auto projection = Projection(camera, aspect);
            Require(std::abs(TransformPoint(projection, {0, 0, -camera.near_}).z_ + 1) < 1e-9 &&
                            std::abs(TransformPoint(projection, {0, 0, -camera.far_}).z_ - 1) <
                                    1e-9,
                    "canonical near/far clip depths");
            const auto height =
                    kind == ProjectionKind::kPerspective
                            ? std::tan(camera.vertical_fov_ * std::numbers::pi / 360) * 3
                            : camera.orthographic_height_ * 0.5;
            const auto corner = TransformPoint(projection, {height * aspect, height, -3});
            Require(std::abs(corner.x_ - 1) < 1e-9 && std::abs(corner.y_ - 1) < 1e-9,
                    "portrait and landscape preserve vertical field of view");
        }
    const auto root = Compose({2, 0, 0}, {0, 0, std::sqrt(0.5), std::sqrt(0.5)}, {1, 1, 1});
    const auto child = Compose({1, 0, 0}, {}, {2, 3, 1});
    const auto position = TransformPoint(Multiply(root, child), {1, 0, 0});
    Require(std::abs(position.x_ - 2) < 1e-9 && std::abs(position.y_ - 3) < 1e-9,
            "parent translation/rotation composes before child scale");
    const auto normal = TransformNormal(child, Normalize({1, 1, 0}));
    const Vector3 tangent{2, -3, 0};
    Require(std::abs(Dot(normal, tangent)) < 1e-9,
            "inverse transpose preserves normal perpendicularity");
    bool rejected = false;
    camera.up_ = {0, 0, 1};
    try {
        View(camera);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    Require(rejected, "parallel look/up vectors reject");
    auto model = Cube();
    Validate(model);
    Require(model.meshes_[0].vertices_.size() == 24 && model.meshes_[0].indices_.size() == 36,
            "cube shares geometry but retains hard face normals");
    model.nodes_[0].local_ = child;
    model.nodes_[0].parent_ = 99;
    model.nodes_.push_back({99, {}, root, {}, false, "Parent"});
    const auto worlds = WorldTransforms(model);
    Require(worlds.at(1).transform_ == Multiply(root, child) && !worlds.at(1).visible_,
            "unordered hierarchy inherits transforms and visibility");
    model.nodes_[1].parent_ = 1;
    rejected = false;
    try {
        Validate(model);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    Require(rejected, "hierarchy cycle rejects");
    model.nodes_.clear();
    for (NodeId id = 1; id <= 65; ++id)
        model.nodes_.push_back({id, id == 1 ? std::nullopt : std::optional<NodeId>(id - 1)});
    rejected = false;
    try {
        Validate(model);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    Require(rejected, "hierarchy depth limit also applies to parent-first ordering");
    std::cout << "scene math: handedness, clip depth, aspect ratio, hierarchy and normals passed\n";
}
}  // namespace
int main() {
    try {
        Run();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
