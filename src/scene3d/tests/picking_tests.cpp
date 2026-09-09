#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "rhythm/scene/picking.h"
#include "rhythm/scene/pose.h"

namespace {
void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void Run() {
    using namespace rhythm::scene;
    Camera camera;
    camera.near_ = .1;
    camera.far_ = 10;
    const auto center = CameraRay(camera, 2, .5, .5);
    Check(center && std::abs(center->near_.z_ - 2.9) < 1e-8 && std::abs(center->far_.z_ + 7) < 1e-8,
          "camera clipping segment");
    const auto corner = CameraRay(camera, 2, 1, 0);
    Check(corner && corner->near_.x_ > 0 && corner->near_.y_ > 0 &&
                  corner->far_.x_ > corner->near_.x_,
          "perspective +Y-down coordinates");
    camera.kind_ = ProjectionKind::kOrthographic;
    const auto orthographic = CameraRay(camera, 2, 1, 0);
    Check(orthographic && std::abs(orthographic->near_.x_ - 2) < 1e-8 &&
                  std::abs(orthographic->near_.y_ - 1) < 1e-8 &&
                  std::abs(orthographic->far_.x_ - 2) < 1e-8,
          "parallel ortho ray");
    Check(!CameraRay(camera, 2, -.01, .5), "outside image rejected");
    camera.near_ = -1;
    Check(!CameraRay(camera, 2, .5, .5), "invalid camera rejected");
    auto model = std::make_shared<Model>(Cube());
    auto geometry = std::make_shared<Geometry>();
    geometry->model_ = model;
    Scene scene;
    scene.instances_.push_back({geometry, ComposeEuler({{0, 0, -2}}), {}, {1, 2, 0, 0}});
    scene.instances_.push_back({geometry, {}, {}, {3, 4, 27, 8}});
    const auto picked = PickScene(scene, *center);
    Check(picked.error_.empty() && picked.hit_ && picked.hit_->instance_ == 1 &&
                  picked.hit_->origin_ == InstanceOrigin{3, 4, 27, 8} &&
                  std::abs(picked.hit_->position_.z_ - .5) < 1e-8,
          "nearest object retains point and graph identities");
    scene.instances_[1].transform_ = ComposeEuler({{0, 0, 1}, {0, 0, 38}, {-2, .7, 1}});
    Check(PickScene(scene, *center).hit_->instance_ == 1, "mirrored nonuniform parent selection");
    scene.instances_[1].material_ = Material{};
    scene.instances_[1].material_->base_color_.alpha_ = 0;
    Check(PickScene(scene, *center).hit_->instance_ == 0, "transparent instance omitted");
    scene.instances_[1].material_.reset();
    scene.instances_[1].transform_ = ComposeEuler({{0, 0, 5}});
    Check(PickScene(scene, *center).hit_->instance_ == 0, "behind near clip omitted");
    scene.instances_[1].transform_ = ComposeEuler({{0, 0, -9}});
    Check(PickScene(scene, *center).hit_->instance_ == 0, "beyond far clip omitted");
    model->nodes_[0].visible_ = false;
    Check(!PickScene(scene, *center).hit_, "hidden model node omitted");
    model->nodes_[0].visible_ = true;
    PickBudget budget;
    budget.triangles_ = 1;
    const auto exhausted = PickScene(scene, *center, budget);
    Check(!exhausted.hit_ && exhausted.error_ == "scene_pick.budget",
          "triangle budget never returns partial hit");
    geometry->deformations_.push_back({1});
    const auto deformed = PickScene(scene, *center);
    Check(!deformed.hit_ && deformed.error_ == "scene_pick.deformation",
          "GPU deformation is explicit");
    geometry->deformations_.clear();
    scene.instances_.clear();
    for (std::uint64_t index = 0; index < 8192; ++index)
        scene.instances_.push_back(
                {geometry, ComposeEuler({{double(index) * 2, 0, 0}}), {}, {12, 14, index + 1, 19}});
    const auto batch = PickScene(scene, *center);
    Check(batch.error_.empty() && batch.hit_ && batch.hit_->origin_.element_ == 1 &&
                  batch.triangles_ == 12,
          "shared mesh bounds avoid 8192 triangle scans");
    budget = {};
    budget.instances_ = 8191;
    Check(PickScene(scene, *center, budget).error_ == "scene_pick.budget",
          "instance budget enforced");
    // A late unsupported object invalidates an earlier otherwise valid hit.
    auto unsupported = std::make_shared<Geometry>(*geometry);
    unsupported->deformations_.push_back({1});
    scene.instances_.back().geometry_ = unsupported;
    Check(!PickScene(scene, *center).hit_, "no partial nearest result after late failure");
    PickRay invalid{{std::numeric_limits<double>::quiet_NaN(), 0, 0}, {1, 0, 0}};
    Check(PickScene(scene, invalid).error_ == "scene_pick.ray", "invalid ray rejected");
}
}  // namespace
int main() {
    try {
        Run();
        std::cout << "Scene picking: clip rays, closest authored identity, culling, bounds and "
                     "budgets passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
