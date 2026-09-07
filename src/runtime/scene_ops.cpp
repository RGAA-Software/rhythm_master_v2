#include "scene_ops.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

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
        case Operation::kGeometrySphere: {
            auto model =
                    instruction.operation_ == Operation::kGeometryCube
                            ? scene::Cube()
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
                     instruction.inputs_.at(1) ? input(1).material_ : std::nullopt});
            output.scene_ = std::make_shared<const scene::Scene>(std::move(scene));
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
        case Operation::kSceneTransform:
        case Operation::kSceneMerge: {
            if (!input(0).scene_) throw std::invalid_argument("runtime.scene_input");
            scene::Scene scene = *input(0).scene_;
            if (instruction.operation_ == Operation::kSceneMerge) {
                if (!input(1).scene_ ||
                    scene.instances_.size() + input(1).scene_->instances_.size() > 256 ||
                    scene.lights_.size() + input(1).scene_->lights_.size() > 4)
                    throw std::length_error("runtime.scene_instances");
                scene.instances_.insert(scene.instances_.end(), input(1).scene_->instances_.begin(),
                                        input(1).scene_->instances_.end());
                scene.lights_.insert(scene.lights_.end(), input(1).scene_->lights_.begin(),
                                     input(1).scene_->lights_.end());
            } else {
                const auto angle = [&](std::size_t port, std::string_view key) {
                    return control(port, key, 0, -36000, 36000) * std::numbers::pi / 360;
                };
                const auto x = angle(1, "rotation_x"), y = angle(2, "rotation_y"),
                           z = angle(3, "rotation_z");
                const auto scale = control(4, "scale", 1, 0.001, 100);
                // Intrinsic X, then Y, then Z; GLM-backed Compose/Multiply own all matrix math.
                const auto rx =
                        scene::Compose({}, {std::sin(x), 0, 0, std::cos(x)}, {scale, scale, scale});
                const auto ry = scene::Compose({}, {0, std::sin(y), 0, std::cos(y)}, {1, 1, 1});
                const auto rz = scene::Compose({control(5, "translate_x", 0, -1000, 1000),
                                                control(6, "translate_y", 0, -1000, 1000),
                                                control(7, "translate_z", 0, -1000, 1000)},
                                               {0, 0, std::sin(z), std::cos(z)}, {1, 1, 1});
                const auto transform = scene::Multiply(rz, scene::Multiply(ry, rx));
                for (auto& light : scene.lights_) {
                    const auto position = scene::TransformPoint(transform, light.direction_);
                    const auto origin = scene::TransformPoint(transform, {});
                    light.direction_ =
                            scene::Normalize({position.x_ - origin.x_, position.y_ - origin.y_,
                                              position.z_ - origin.z_});
                }
                for (auto& instance : scene.instances_) {
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
            camera.eye_ = {scalar("eye_x", 0), scalar("eye_y", 0), scalar("eye_z", 3)};
            camera.target_ = {scalar("target_x", 0), scalar("target_y", 0), scalar("target_z", 0)};
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
    scene::Scene result;
    if (output.scene_)
        result = *output.scene_;
    else if (output.geometry_)
        result.instances_.push_back({output.geometry_});
    if (output.material_ ||
        (output.scene_ && result.instances_.empty() && !result.lights_.empty())) {
        static const auto kSphere = std::make_shared<const scene::Model>(scene::Sphere());
        auto geometry =
                std::make_shared<const scene::Geometry>(scene::Geometry{output.node_, 0, kSphere});
        scene::Material material;
        material.unlit_ = false;
        result.instances_.push_back({std::move(geometry), {}, output.material_.value_or(material)});
    }
    // Neutral preview lighting never changes the authored graph or final output.
    if (result.lights_.empty()) result.lights_.push_back({scene::Normalize({1, 1, 2}), {3, 3, 3}});
    return result;
}
}  // namespace rhythm::runtime::detail
