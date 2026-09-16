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
render::ShadowFilter Filter(scene::ShadowFilter value) {
    switch (value) {
        case scene::ShadowFilter::kNearest:
            return render::ShadowFilter::kNearest;
        case scene::ShadowFilter::kPcf5:
            return render::ShadowFilter::kPcf5;
        case scene::ShadowFilter::kPcf13:
            return render::ShadowFilter::kPcf13;
    }
    throw std::invalid_argument("runtime.shadow_filter");
}
scene::Vector3 Translate(scene::Vector3 value, scene::Vector3 first_axis, double first_distance,
                         scene::Vector3 second_axis, double second_distance) {
    return {value.x_ + first_axis.x_ * first_distance + second_axis.x_ * second_distance,
            value.y_ + first_axis.y_ * first_distance + second_axis.y_ * second_distance,
            value.z_ + first_axis.z_ * first_distance + second_axis.z_ * second_distance};
}
double Snapped(double value, double unit) { return std::floor(value / unit + 0.5) * unit; }
}  // namespace

scene::Camera ShadowCamera(const scene::Scene& scene) {
    const auto& shadow = scene.shadow_.value();
    if (shadow.light_ >= scene.lights_.size() + scene.positional_lights_.size())
        throw std::invalid_argument("runtime.shadow_light");
    scene::Camera camera;
    scene::Vector3 direction;
    bool directional = false;
    if (shadow.light_ < scene.lights_.size()) {
        directional = true;
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
    if (directional) {
        // Godot snaps both orthographic boundaries to radius * 4 / texture size.
        // For this explicit full-height extent, the equivalent center grid is
        // two shadow texels. Depth stays light-relative and is not quantized.
        const auto unit = shadow.extent_ * 2.0 / shadow.resolution_;
        if (!std::isfinite(unit) || unit < 1e-12)
            throw std::invalid_argument("runtime.shadow_extent");
        const auto right = scene::Normalize(scene::Cross(camera.up_, direction));
        const auto up = scene::Cross(direction, right);
        const auto right_position = scene::Dot(right, camera.target_);
        const auto up_position = scene::Dot(up, camera.target_);
        const auto right_shift = Snapped(right_position, unit) - right_position;
        const auto up_shift = Snapped(up_position, unit) - up_position;
        camera.eye_ = Translate(camera.eye_, right, right_shift, up, up_shift);
        camera.target_ = Translate(camera.target_, right, right_shift, up, up_shift);
    }
    return camera;
}
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
    const auto camera = ShadowCamera(scene);
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
            render::SceneShadow{depth_.Handle(),       Matrix(scene::Multiply(projection, view)),
                                shadow.light_,         shadow.resolution_,
                                shadow.depth_bias_,    shadow.normal_bias_,
                                Filter(shadow.filter_)};
}
}  // namespace rhythm::runtime::detail
