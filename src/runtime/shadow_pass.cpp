#include "shadow_pass.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace rhythm::runtime::detail {
namespace {
render::Matrix4 Matrix(const scene::Matrix& value) {
    render::Matrix4 result;
    std::transform(value.values_.begin(), value.values_.end(), result.begin(),
                   [](double element) { return float(element); });
    return result;
}
scene::Camera Camera(const scene::Scene& scene) {
    const auto& shadow = scene.shadow_.value();
    if (shadow.light_ >= scene.lights_.size() + scene.positional_lights_.size())
        throw std::invalid_argument("runtime.shadow_light");
    scene::Camera camera;
    scene::Vector3 direction;
    if (shadow.light_ < scene.lights_.size()) {
        direction = scene::Normalize(scene.lights_[shadow.light_].direction_);
        camera.eye_ = {shadow.center_.x_ + direction.x_ * shadow.distance_,
                       shadow.center_.y_ + direction.y_ * shadow.distance_,
                       shadow.center_.z_ + direction.z_ * shadow.distance_};
        camera.target_ = shadow.center_;
        camera.kind_ = scene::ProjectionKind::kOrthographic;
        camera.orthographic_height_ = shadow.extent_;
        camera.near_ = 0.001;
        camera.far_ = 2 * shadow.distance_;
    } else {
        const auto& light = scene.positional_lights_[shadow.light_ - scene.lights_.size()];
        if (!light.spot_) throw std::invalid_argument("render.point_shadow_unsupported");
        direction = scene::Normalize(light.direction_);
        camera.eye_ = light.position_;
        camera.target_ = {light.position_.x_ + direction.x_, light.position_.y_ + direction.y_,
                          light.position_.z_ + direction.z_};
        camera.vertical_fov_ = std::max(1.0, 2 * light.cone_angle_);
        camera.near_ = shadow.near_;
        camera.far_ = light.range_;
    }
    // Stable alternative up axis when a light looks almost vertically downward.
    camera.up_ = std::abs(direction.y_) > 0.95 ? scene::Vector3{0, 0, 1} : scene::Vector3{0, 1, 0};
    return camera;
}
}  // namespace
void ShadowPass::Apply(const scene::Scene& scene, render::SceneDrawList& receivers,
                       render::Renderer& renderer) {
    if (!scene.shadow_) {
        color_ = {};
        depth_ = {};
        resolution_ = 0;
        receivers.shadow_.reset();
        return;
    }
    const auto& shadow = *scene.shadow_;
    if (shadow.resolution_ < 256 || shadow.resolution_ > 2048 ||
        (shadow.resolution_ & (shadow.resolution_ - 1)) != 0)
        throw std::invalid_argument("runtime.shadow_resolution");
    const auto camera = Camera(scene);
    const auto view = scene::View(camera);
    const auto projection = scene::Projection(camera, 1);
    if (!renderer.SupportsSampleableDepth())
        throw std::runtime_error("render.sampleable_depth_unsupported");
    if (resolution_ != shadow.resolution_ || !renderer.IsValid(depth_.Handle()) ||
        !renderer.IsValid(color_.Handle())) {
        // Allocate both before replacing the current pair; partial failure is RAII.
        const render::Extent extent{shadow.resolution_, shadow.resolution_};
        auto color = renderer.CreateTexture(extent);
        auto depth = renderer.CreateDepthTexture(extent);
        color_ = std::move(color);
        depth_ = std::move(depth);
        resolution_ = shadow.resolution_;
    }
    render::SceneDrawList casters;
    casters.view_ = Matrix(view);
    casters.projection_ = Matrix(projection);
    for (const auto& receiver : receivers.draws_) {
        if (receiver.color_[3] < 1) continue;
        auto caster = receiver;
        caster.unlit_ = true;
        caster.color_ = {1, 1, 1, 1};
        caster.textures_ = {};
        casters.draws_.push_back(caster);
    }
    renderer.SubmitSceneDepth(color_.Handle(), depth_.Handle(), casters);
    receivers.shadow_ =
            render::SceneShadow{depth_.Handle(),    Matrix(scene::Multiply(projection, view)),
                                shadow.light_,      shadow.resolution_,
                                shadow.depth_bias_, shadow.normal_bias_,
                                shadow.filter_};
}
}  // namespace rhythm::runtime::detail
