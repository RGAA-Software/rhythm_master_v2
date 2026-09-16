#include "shadow_pass.h"

#include <algorithm>
#include <cmath>
#include <numbers>
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
scene::Vector3 Move(scene::Vector3 value, scene::Vector3 axis, double distance) {
    return {value.x_ + axis.x_ * distance, value.y_ + axis.y_ * distance,
            value.z_ + axis.z_ * distance};
}
scene::Camera FitCascade(const scene::Scene::DirectionalLight& light,
                         const scene::ShadowSettings& shadow, const scene::Camera& receiver,
                         double aspect, double near, double far) {
    const auto backward = scene::Normalize({receiver.eye_.x_ - receiver.target_.x_,
                                            receiver.eye_.y_ - receiver.target_.y_,
                                            receiver.eye_.z_ - receiver.target_.z_});
    const auto right = scene::Normalize(scene::Cross(receiver.up_, backward));
    const auto up = scene::Cross(backward, right);
    const auto half_height = [&](double depth) {
        if (receiver.kind_ == scene::ProjectionKind::kOrthographic)
            return receiver.orthographic_height_ * 0.5;
        return std::tan(receiver.vertical_fov_ * std::numbers::pi / 360) * depth;
    };
    std::array<scene::Vector3, 8> endpoints;
    std::size_t endpoint = 0;
    for (const auto depth : {near, far}) {
        const auto center = Move(receiver.eye_, backward, -depth);
        const auto vertical = half_height(depth);
        const auto horizontal = vertical * aspect;
        for (const auto y : {-vertical, vertical})
            for (const auto x : {-horizontal, horizontal})
                endpoints[endpoint++] = Translate(center, right, x, up, y);
    }
    scene::Vector3 center;
    for (const auto& point : endpoints) {
        center.x_ += point.x_ / endpoints.size();
        center.y_ += point.y_ / endpoints.size();
        center.z_ += point.z_ / endpoints.size();
    }
    double radius = 0;
    for (const auto& point : endpoints)
        radius = std::max(radius, std::hypot(point.x_ - center.x_, point.y_ - center.y_,
                                             point.z_ - center.z_));
    // Godot expands by one texel on each side before snapping the projection.
    radius *= double(shadow.resolution_) / (shadow.resolution_ - 2.0);
    const auto direction = scene::Normalize(light.direction_);
    const auto camera_up =
            std::abs(direction.y_) > 0.95 ? scene::Vector3{0, 0, 1} : scene::Vector3{0, 1, 0};
    const auto light_right = scene::Normalize(scene::Cross(camera_up, direction));
    const auto light_up = scene::Cross(direction, light_right);
    const auto unit = radius * 4.0 / shadow.resolution_;
    if (!std::isfinite(unit) || unit < 1e-12)
        throw std::invalid_argument("runtime.shadow_cascade_extent");
    center = Translate(
            center, light_right,
            Snapped(scene::Dot(light_right, center), unit) - scene::Dot(light_right, center),
            light_up, Snapped(scene::Dot(light_up, center), unit) - scene::Dot(light_up, center));
    const auto depth_distance = radius + shadow.distance_;
    scene::Camera camera;
    camera.eye_ = Move(center, direction, depth_distance);
    camera.target_ = center;
    camera.up_ = camera_up;
    camera.kind_ = scene::ProjectionKind::kOrthographic;
    camera.orthographic_height_ = radius * 2;
    camera.near_ = 0.001;
    camera.far_ = depth_distance * 2;
    return camera;
}
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
DirectionalCascadeCameras CascadeCameras(const scene::Scene& scene,
                                         const scene::Camera& receiver_camera, double aspect) {
    const auto& shadow = scene.shadow_.value();
    if (shadow.light_ >= scene.lights_.size())
        throw std::invalid_argument("runtime.directional_shadow_cascades");
    if (!std::isfinite(shadow.cascade_split_) || shadow.cascade_split_ < 0.05 ||
        shadow.cascade_split_ > 0.95 || !std::isfinite(shadow.max_distance_) ||
        shadow.max_distance_ <= 0)
        throw std::invalid_argument("runtime.shadow_cascade_settings");
    (void)scene::Projection(receiver_camera, aspect);
    const auto maximum = std::max(std::min(receiver_camera.far_, shadow.max_distance_),
                                  receiver_camera.near_ + 0.001);
    DirectionalCascadeCameras result;
    result.split_depth_ =
            receiver_camera.near_ + (maximum - receiver_camera.near_) * shadow.cascade_split_;
    result.cameras_[0] = FitCascade(scene.lights_[shadow.light_], shadow, receiver_camera, aspect,
                                    receiver_camera.near_, result.split_depth_);
    result.cameras_[1] = FitCascade(scene.lights_[shadow.light_], shadow, receiver_camera, aspect,
                                    result.split_depth_, maximum);
    return result;
}
std::array<scene::Camera, 6> PointShadowCameras(const scene::Scene& scene) {
    const auto& shadow = scene.shadow_.value();
    if (shadow.light_ < scene.lights_.size() ||
        shadow.light_ >= scene.lights_.size() + scene.positional_lights_.size())
        throw std::invalid_argument("runtime.point_shadow_light");
    const auto& light = scene.positional_lights_[shadow.light_ - scene.lights_.size()];
    if (light.spot_) throw std::invalid_argument("runtime.point_shadow_light");
    if (!std::isfinite(shadow.near_) || shadow.near_ < 0.001 || shadow.near_ >= light.range_)
        throw std::invalid_argument("runtime.point_shadow_range");
    // Matches Godot 4.5.1 RendererSceneCull's cube face order and up vectors.
    constexpr std::array<scene::Vector3, 6> kDirections{
            {{1, 0, 0}, {-1, 0, 0}, {0, -1, 0}, {0, 1, 0}, {0, 0, 1}, {0, 0, -1}}};
    constexpr std::array<scene::Vector3, 6> kUp{
            {{0, -1, 0}, {0, -1, 0}, {0, 0, -1}, {0, 0, 1}, {0, -1, 0}, {0, -1, 0}}};
    std::array<scene::Camera, 6> result;
    for (std::size_t face = 0; face < result.size(); ++face) {
        auto& camera = result[face];
        camera.eye_ = light.position_;
        camera.target_ = Move(light.position_, kDirections[face], 1);
        camera.up_ = kUp[face];
        camera.vertical_fov_ = 90;
        camera.near_ = shadow.near_;
        camera.far_ = light.range_;
    }
    return result;
}
void ShadowPass::Apply(const scene::Scene& scene, const scene::Camera& receiver_camera,
                       double aspect, render::SceneDrawList& receivers,
                       render::Renderer& renderer) {
    if (!scene.shadow_) {
        colors_ = {};
        depths_ = {};
        resolution_ = 0;
        passes_ = 0;
        receivers.shadow_.reset();
        return;
    }
    const auto& shadow = *scene.shadow_;
    if (shadow.resolution_ < 256 || shadow.resolution_ > 2048 ||
        (shadow.resolution_ & (shadow.resolution_ - 1)) != 0)
        throw std::invalid_argument("runtime.shadow_resolution");
    if (shadow.cascades_ < 1 || shadow.cascades_ > 2)
        throw std::invalid_argument("runtime.shadow_cascades");
    const bool positional = shadow.light_ >= scene.lights_.size() &&
                            shadow.light_ < scene.lights_.size() + scene.positional_lights_.size();
    const bool point =
            positional && !scene.positional_lights_[shadow.light_ - scene.lights_.size()].spot_;
    if (point && shadow.cascades_ != 1)
        throw std::invalid_argument("runtime.point_shadow_cascades");
    const std::uint8_t passes = point ? 6 : shadow.cascades_;
    std::array<scene::Camera, 6> cameras;
    double cascade_split = 0;
    if (point) {
        cameras = PointShadowCameras(scene);
    } else if (shadow.cascades_ == 2) {
        const auto fitted = CascadeCameras(scene, receiver_camera, aspect);
        std::copy(fitted.cameras_.begin(), fitted.cameras_.end(), cameras.begin());
        cascade_split = fitted.split_depth_;
    } else {
        cameras[0] = ShadowCamera(scene);
    }
    if (!renderer.SupportsSampleableDepth())
        throw std::runtime_error("render.sampleable_depth_unsupported");
    bool valid = resolution_ == shadow.resolution_ && passes_ == passes;
    for (std::size_t index = 0; valid && index < passes; ++index)
        valid = renderer.IsValid(depths_[index].Handle()) &&
                renderer.IsValid(colors_[index].Handle());
    if (!valid) {
        // Allocate every required attachment before replacing the current set;
        // partial allocation failure remains RAII and cannot publish half a shadow set.
        const render::Extent extent{shadow.resolution_, shadow.resolution_};
        std::array<render::Texture, 6> colors;
        std::array<render::Texture, 6> depths;
        for (std::size_t index = 0; index < passes; ++index) {
            colors[index] = renderer.CreateTexture(extent);
            depths[index] = renderer.CreateDepthTexture(extent);
        }
        colors_ = std::move(colors);
        depths_ = std::move(depths);
        resolution_ = shadow.resolution_;
        passes_ = passes;
    }
    render::SceneDrawList casters;
    for (const auto& receiver : receivers.draws_) {
        if (receiver.color_[3] < 1) continue;
        auto caster = receiver;
        caster.unlit_ = true;
        caster.color_ = {1, 1, 1, 1};
        caster.textures_ = {};
        casters.draws_.push_back(caster);
    }
    std::array<render::Matrix4, 6> matrices;
    for (std::size_t index = 0; index < passes; ++index) {
        const auto view = scene::View(cameras[index]);
        const auto projection = scene::Projection(cameras[index], 1);
        casters.view_ = Matrix(view);
        casters.projection_ = Matrix(projection);
        renderer.SubmitSceneDepth(colors_[index].Handle(), depths_[index].Handle(), casters);
        matrices[index] = Matrix(scene::Multiply(projection, view));
    }
    render::SceneShadow rendered;
    rendered.light_ = shadow.light_;
    rendered.resolution_ = shadow.resolution_;
    rendered.depth_bias_ = shadow.depth_bias_;
    rendered.normal_bias_ = shadow.normal_bias_;
    rendered.filter_ = Filter(shadow.filter_);
    if (point) {
        for (std::size_t face = 0; face < rendered.point_depths_.size(); ++face) {
            rendered.point_depths_[face] = depths_[face].Handle();
            rendered.point_world_to_clip_[face] = matrices[face];
        }
    } else {
        rendered.depth_ = depths_[0].Handle();
        rendered.world_to_clip_ = matrices[0];
    }
    if (!point && shadow.cascades_ == 2) {
        rendered.cascade_depth_ = depths_[1].Handle();
        rendered.cascade_world_to_clip_ = matrices[1];
        rendered.cascade_split_ = static_cast<float>(cascade_split);
    }
    receivers.shadow_ = rendered;
}
}  // namespace rhythm::runtime::detail
