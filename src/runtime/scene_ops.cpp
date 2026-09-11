#include "scene_ops.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "rhythm/scene/pose.h"

namespace rhythm::runtime::detail {
void EvaluateScene(const graph::Instruction& instruction, std::span<const NodeOutput> outputs,
                   NodeOutput& output, const scene::Resources& resources) {
    const auto& node = instruction.node_;
    const auto scalar = [&](std::string_view key, double fallback) {
        return graph::Scalar(node, key, fallback);
    };
    const auto input = [&](std::size_t port) -> const NodeOutput& {
        if (port >= instruction.inputs_.size() || !instruction.inputs_[port] ||
            *instruction.inputs_[port] >= outputs.size())
            throw std::invalid_argument("runtime.scene_input");
        return outputs[*instruction.inputs_[port]];
    };
    const auto control = [&](std::size_t port, std::string_view key, double fallback,
                             double minimum, double maximum) {
        const auto value =
                instruction.inputs_.at(port) ? input(port).scalar_ : scalar(key, fallback);
        return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
    };
    using graph::Operation;
    switch (instruction.operation_) {
        case Operation::kGeometryGlb: {
            const auto& id = std::get<assets::AssetId>(node.properties_.at("asset"));
            const auto& resource = scene::FindModel(resources, id);
            output.geometry_ = std::make_shared<const scene::Geometry>(
                    scene::Geometry{node.id_, output.version_, resource.model_});
            break;
        }
        case Operation::kGeometryCube:
        case Operation::kGeometryTorus:
        case Operation::kGeometrySphere: {
            auto model =
                    instruction.operation_ == Operation::kGeometryCube ? scene::Cube()
                    : instruction.operation_ == Operation::kGeometryTorus
                            ? scene::Torus(
                                      static_cast<float>(scalar("radius", 0.8)),
                                      static_cast<float>(scalar("tube_ratio", 0.08)),
                                      static_cast<std::uint32_t>(scalar("radial_segments", 64)),
                                      static_cast<std::uint32_t>(scalar("tube_segments", 16)))
                            : scene::Sphere(
                                      static_cast<float>(scalar("radius", 0.5)),
                                      static_cast<float>(scalar("height", 1)),
                                      static_cast<std::uint32_t>(scalar("radial_segments", 32)),
                                      static_cast<std::uint32_t>(scalar("rings", 16)));
            output.geometry_ = std::make_shared<const scene::Geometry>(
                    scene::Geometry{node.id_, output.version_,
                                    std::make_shared<const scene::Model>(std::move(model))});
            break;
        }
        case Operation::kMaterialTextures: {
            if (!input(0).material_) throw std::invalid_argument("runtime.material_input");
            auto material = *input(0).material_;
            for (std::size_t slot = 0; slot < 4; ++slot)
                if (instruction.inputs_.at(slot + 1))
                    material.textures_.nodes_[slot] = input(slot + 1).node_;
            material.textures_.color_srgb_ = scalar("material_srgb", 1) != 0;
            material.textures_.normal_scale_ = float(scalar("normal_scale", 1));
            material.textures_.uv_transform_ = {
                    float(scalar("uv_scale_x", 1)), float(scalar("uv_scale_y", 1)),
                    float(scalar("uv_offset_x", 0)), float(scalar("uv_offset_y", 0))};
            output.material_ = material;
            break;
        }
        case Operation::kMaterialUnlit:
        case Operation::kMaterialPbr: {
            const auto color = graph::ColorValue(node, "color_a",
                                                 instruction.operation_ == Operation::kMaterialPbr
                                                         ? graph::Color{0.8, 0.3, 0.08, 1}
                                                         : graph::Color{0.1, 0.8, 1, 1});
            scene::Material material;
            material.base_color_ = {static_cast<float>(color.r_), static_cast<float>(color.g_),
                                    static_cast<float>(color.b_), static_cast<float>(color.a_)};
            material.double_sided_ = scalar("double_sided", 0) != 0;
            material.alpha_depth_prepass_ = scalar("alpha_depth_prepass", 0) != 0;
            material.render_priority_ = static_cast<std::int32_t>(scalar("render_priority", 0));
            if (instruction.operation_ == Operation::kMaterialPbr) {
                material.unlit_ = false;
                material.metallic_ = static_cast<float>(control(0, "metallic", 0, 0, 1));
                material.roughness_ = static_cast<float>(control(1, "roughness", 0.5, 0.05, 1));
                const auto emission = control(2, "emission", 0, 0, 100);
                const auto tint = graph::ColorValue(node, "color_b", {1, 1, 1, 1});
                material.emissive_ = {emission * tint.r_, emission * tint.g_, emission * tint.b_};
            }
            output.material_ = material;
            break;
        }
        case Operation::kSceneInstance: {
            if (!input(0).geometry_) throw std::invalid_argument("runtime.geometry");
            scene::Scene scene;
            scene.instances_.push_back(
                    {input(0).geometry_,
                     {},
                     instruction.inputs_.at(1) ? input(1).material_ : std::nullopt,
                     {node.id_},
                     scalar("sorting_offset", 0)});
            output.scene_ = std::make_shared<const scene::Scene>(std::move(scene));
            break;
        }
        case Operation::kPointLight:
        case Operation::kSpotLight: {
            scene::Scene result;
            scene::Scene::PositionalLight light;
            const auto energy = control(0, "light_energy", 8, 0, 100);
            const auto color = graph::ColorValue(node, "color_a", {1, 1, 1, 1});
            light.radiance_ = {color.r_ * energy, color.g_ * energy, color.b_ * energy};
            light.position_ = {control(1, "translate_x", 0, -10000, 10000),
                               control(2, "translate_y", 2, -10000, 10000),
                               control(3, "translate_z", 3, -10000, 10000)};
            light.range_ = scalar("light_range", 10);
            light.decay_ = scalar("light_decay", 2);
            light.spot_ = instruction.operation_ == Operation::kSpotLight;
            if (light.spot_) {
                light.direction_ = scene::Normalize(
                        {scalar("light_x", 0), scalar("light_y", -0.5), scalar("light_z", -1)});
                light.cone_angle_ = control(4, "spot_angle", 45, 0.1, 89);
                light.cone_decay_ = scalar("spot_decay", 1);
            }
            result.positional_lights_.push_back(light);
            output.scene_ = std::make_shared<const scene::Scene>(std::move(result));
            break;
        }
        case Operation::kDirectionalLight: {
            const auto direction = scene::Normalize(
                    {scalar("light_x", 1), scalar("light_y", 1), scalar("light_z", 1)});
            const auto energy = control(0, "light_energy", 3, 0, 100);
            const auto color = graph::ColorValue(node, "color_a", {1, 1, 1, 1});
            scene::Scene scene;
            scene.lights_.push_back(
                    {direction, {color.r_ * energy, color.g_ * energy, color.b_ * energy}});
            output.scene_ = std::make_shared<const scene::Scene>(std::move(scene));
            break;
        }
        case Operation::kSceneEnvironment: {
            if (!input(0).scene_) throw std::invalid_argument("runtime.scene_input");
            auto result = *input(0).scene_;
            result.environment_.reset();
            if (scalar("environment_enabled", 1) != 0)
                result.environment_ = scene::EnvironmentSettings{
                        input(1).node_, float(control(2, "environment_energy", 1, 0, 100)),
                        float(control(3, "environment_rotation", 0, -36000, 36000)),
                        scalar("environment_srgb", 1) != 0};
            output.scene_ = std::make_shared<const scene::Scene>(std::move(result));
            break;
        }
        case Operation::kSceneShadow: {
            if (!input(0).scene_) throw std::invalid_argument("runtime.scene_input");
            auto result = *input(0).scene_;
            result.shadow_.reset();
            if (scalar("shadow_enabled", 1) != 0) {
                scene::ShadowSettings shadow;
                shadow.light_ = std::uint32_t(scalar("shadow_light", 0));
                constexpr std::array<std::uint16_t, 4> kResolutions{256, 512, 1024, 2048};
                shadow.resolution_ = kResolutions.at(std::size_t(scalar("shadow_resolution", 2)));
                shadow.center_ = {scalar("shadow_center_x", 0), scalar("shadow_center_y", 0),
                                  scalar("shadow_center_z", 0)};
                shadow.extent_ = scalar("shadow_extent", 10);
                shadow.distance_ = scalar("shadow_distance", 20);
                shadow.near_ = scalar("shadow_near", 0.05);
                shadow.depth_bias_ = float(scalar("shadow_bias", 0.001));
                shadow.normal_bias_ = float(scalar("shadow_normal_bias", 0.01));
                shadow.filter_ = scalar("shadow_filter", 1) != 0;
                result.shadow_ = shadow;
            }
            output.scene_ = std::make_shared<const scene::Scene>(std::move(result));
            break;
        }
        case Operation::kSceneTransform:
        case Operation::kSceneMerge: {
            if (!input(0).scene_) throw std::invalid_argument("runtime.scene_input");
            scene::Scene scene = *input(0).scene_;
            if (instruction.operation_ == Operation::kSceneMerge) {
                if (!input(1).scene_ ||
                    scene.instances_.size() + input(1).scene_->instances_.size() >
                            graph::kMaximumSceneInstances ||
                    scene.lights_.size() + scene.positional_lights_.size() +
                                    input(1).scene_->lights_.size() +
                                    input(1).scene_->positional_lights_.size() >
                            4)
                    throw std::length_error("runtime.scene_instances");
                const auto& second = *input(1).scene_;
                if (scene.environment_ && second.environment_)
                    throw std::invalid_argument("runtime.multiple_environments");
                if (second.environment_) scene.environment_ = second.environment_;
                if (scene.shadow_ && second.shadow_)
                    throw std::invalid_argument("runtime.multiple_shadows");
                if (scene.shadow_ && scene.shadow_->light_ >= scene.lights_.size())
                    scene.shadow_->light_ += std::uint32_t(second.lights_.size());
                else if (second.shadow_) {
                    scene.shadow_ = second.shadow_;
                    const auto offset =
                            scene.shadow_->light_ < second.lights_.size()
                                    ? scene.lights_.size()
                                    : scene.lights_.size() + scene.positional_lights_.size();
                    scene.shadow_->light_ += std::uint32_t(offset);
                }
                scene.instances_.insert(scene.instances_.end(), input(1).scene_->instances_.begin(),
                                        input(1).scene_->instances_.end());
                scene.lights_.insert(scene.lights_.end(), input(1).scene_->lights_.begin(),
                                     input(1).scene_->lights_.end());
                scene.positional_lights_.insert(scene.positional_lights_.end(),
                                                input(1).scene_->positional_lights_.begin(),
                                                input(1).scene_->positional_lights_.end());
            } else {
                const auto angle = [&](std::size_t port, std::string_view key) {
                    return control(port, key, 0, -36000, 36000);
                };
                const auto x = angle(1, "rotation_x"), y = angle(2, "rotation_y"),
                           z = angle(3, "rotation_z");
                const auto scale = control(4, "scale", 1, 0.001, 100);
                // Axis factors multiply uniform scale. Bound the product as well so a
                // single transform retains the existing invertible scale range.
                const auto axis = [&](std::size_t port, std::string_view key) {
                    return std::clamp(scale * control(port, key, 1, 0.001, 100), 0.001, 100.0);
                };
                const auto transform = scene::ComposeEuler(
                        {{control(5, "translate_x", 0, -1000, 1000),
                          control(6, "translate_y", 0, -1000, 1000),
                          control(7, "translate_z", 0, -1000, 1000)},
                         {x, y, z},
                         {axis(8, "scale_x"), axis(9, "scale_y"), axis(10, "scale_z")}});
                if (scene.shadow_)
                    scene.shadow_->center_ =
                            scene::TransformPoint(transform, scene.shadow_->center_);
                for (auto& light : scene.positional_lights_) {
                    light.position_ = scene::TransformPoint(transform, light.position_);
                    const auto tip = scene::TransformPoint(transform, light.direction_);
                    const auto origin = scene::TransformPoint(transform, {});
                    light.direction_ = scene::Normalize(
                            {tip.x_ - origin.x_, tip.y_ - origin.y_, tip.z_ - origin.z_});
                }
                for (auto& light : scene.lights_) {
                    const auto position = scene::TransformPoint(transform, light.direction_);
                    const auto origin = scene::TransformPoint(transform, {});
                    light.direction_ =
                            scene::Normalize({position.x_ - origin.x_, position.y_ - origin.y_,
                                              position.z_ - origin.z_});
                }
                for (auto& instance : scene.instances_) {
                    if (!instance.origin_.transform_) instance.origin_.transform_ = node.id_;
                    instance.transform_ = scene::Multiply(transform, instance.transform_);
                    if (!scene::ValidAffine(instance.transform_))
                        throw std::invalid_argument("runtime.scene_transform");
                }
            }
            output.scene_ = std::make_shared<const scene::Scene>(std::move(scene));
            break;
        }
        case Operation::kSceneCamera: {
            scene::Camera camera;
            camera.eye_ = {control(0, "eye_x", 0, -10000, 10000),
                           control(1, "eye_y", 0, -10000, 10000),
                           control(2, "eye_z", 3, -10000, 10000)};
            camera.target_ = {control(3, "target_x", 0, -10000, 10000),
                              control(4, "target_y", 0, -10000, 10000),
                              control(5, "target_z", 0, -10000, 10000)};
            camera.kind_ = scalar("projection", 0) == 0 ? scene::ProjectionKind::kPerspective
                                                        : scene::ProjectionKind::kOrthographic;
            camera.vertical_fov_ = scalar("field_of_view", 60);
            camera.orthographic_height_ = scalar("orthographic_height", 2);
            camera.near_ = scalar("near_plane", 0.05);
            camera.far_ = scalar("far_plane", 1000);
            (void)scene::View(camera);
            (void)scene::Projection(camera, 1);
            output.camera_ = camera;
            break;
        }
        default:
            throw std::invalid_argument("runtime.scene_operation");
    }
}
std::vector<graph::GeometryBudget> GeometryBudgets(const graph::ExecutionPlan& plan,
                                                   const scene::Resources& resources) {
    std::vector<graph::GeometryBudget> result;
    for (const auto& instruction : plan.instructions_) {
        if (instruction.operation_ != graph::Operation::kGeometryGlb) continue;
        const auto& model = scene::FindModel(
                resources, std::get<assets::AssetId>(instruction.node_.properties_.at("asset")));
        result.push_back({instruction.node_.id_, model.vertices_, model.indices_,
                          model.draw_indices_, model.draws_});
    }
    return result;
}
scene::Scene PreviewScene(const NodeOutput& output) {
    if (output.path_) return PreviewPath(output);
    scene::Scene result;
    if (output.scene_)
        result = *output.scene_;
    else if (output.geometry_)
        result.instances_.push_back({output.geometry_});
    if (output.material_ ||
        (output.scene_ && result.instances_.empty() &&
         (!result.lights_.empty() || !result.positional_lights_.empty() || result.environment_))) {
        static const auto kSphere = std::make_shared<const scene::Model>(scene::Sphere());
        auto geometry =
                std::make_shared<const scene::Geometry>(scene::Geometry{output.node_, 0, kSphere});
        scene::Material material;
        material.unlit_ = false;
        result.instances_.push_back({std::move(geometry), {}, output.material_.value_or(material)});
    }
    // Neutral preview lighting never changes the authored graph or final output.
    if (result.lights_.empty() && result.positional_lights_.empty() && !result.environment_)
        result.lights_.push_back({scene::Normalize({1, 1, 2}), {3, 3, 3}});
    return result;
}
}  // namespace rhythm::runtime::detail
