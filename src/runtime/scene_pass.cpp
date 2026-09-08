#include "scene_pass.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace rhythm::runtime::detail {
namespace {
render::Matrix4 Matrix(const scene::Matrix& source) {
    render::Matrix4 result;
    std::transform(source.values_.begin(), source.values_.end(), result.begin(),
                   [](double value) { return static_cast<float>(value); });
    return result;
}
}  // namespace
render::SceneDrawList ScenePass::Build(const scene::Scene& scene, const scene::Camera& camera,
                                       render::Extent extent, render::Renderer& renderer) {
    if (!extent.width_ || !extent.height_ ||
        scene.instances_.size() > graph::kMaximumSceneInstances || scene.lights_.size() > 4 ||
        !renderer.SupportsScenes())
        throw std::invalid_argument("runtime.scene");
    const auto view = scene::View(camera);
    render::SceneDrawList result;
    result.view_ = Matrix(view);
    result.camera_backward_ = {static_cast<float>(view.values_[2]),
                               static_cast<float>(view.values_[6]),
                               static_cast<float>(view.values_[10])};
    result.orthographic_ = camera.kind_ == scene::ProjectionKind::kOrthographic;
    result.projection_ = Matrix(scene::Projection(camera, double(extent.width_) / extent.height_));
    result.camera_position_ = {static_cast<float>(camera.eye_.x_),
                               static_cast<float>(camera.eye_.y_),
                               static_cast<float>(camera.eye_.z_)};
    for (const auto& light : scene.lights_)
        result.lights_.push_back(
                {{static_cast<float>(light.direction_.x_), static_cast<float>(light.direction_.y_),
                  static_cast<float>(light.direction_.z_)},
                 {static_cast<float>(light.radiance_.x_), static_cast<float>(light.radiance_.y_),
                  static_cast<float>(light.radiance_.z_)}});
    std::set<Key> active;
    for (const auto& instance : scene.instances_) {
        if (!instance.geometry_ || !instance.geometry_->model_ || !instance.geometry_->id_ ||
            !scene::ValidAffine(instance.transform_))
            throw std::invalid_argument("runtime.geometry");
        active.emplace(instance.geometry_->id_, instance.geometry_->revision_);
    }
    std::erase_if(uploads_, [&](const auto& item) { return !active.contains(item.first); });
    std::uint64_t index_count = 0;
    for (const auto& instance : scene.instances_) {
        const auto& geometry = *instance.geometry_;
        const Key key{geometry.id_, geometry.revision_};
        if (!uploads_.contains(key)) {
            scene::Validate(*geometry.model_);
            Uploaded upload;
            upload.model_ = geometry.model_;
            upload.worlds_ = scene::WorldTransforms(*geometry.model_);
            for (const auto& mesh : geometry.model_->meshes_) {
                std::vector<render::MeshVertex> vertices;
                vertices.reserve(mesh.vertices_.size());
                for (const auto& v : mesh.vertices_)
                    vertices.push_back(
                            {v.x_, v.y_, v.z_, v.normal_x_, v.normal_y_, v.normal_z_, v.u_, v.v_});
                upload.meshes_.push_back(renderer.CreateMesh(vertices, mesh.indices_));
            }
            uploads_.emplace(key, std::move(upload));
        }
        const auto& upload = uploads_.at(key);
        for (const auto& node : upload.model_->nodes_) {
            const auto& world = upload.worlds_.at(node.id_);
            if (!world.visible_) continue;
            const auto transform = scene::Multiply(instance.transform_, world.transform_);
            if (!scene::ValidAffine(transform))
                throw std::invalid_argument("runtime.scene_transform");
            for (const auto mesh_index : node.meshes_) {
                const auto& mesh = upload.model_->meshes_.at(mesh_index);
                const auto material =
                        instance.material_.value_or(upload.model_->materials_.at(mesh.material_));
                index_count += mesh.indices_.size();
                if (result.draws_.size() >= 16384 || index_count > 3000000)
                    throw std::length_error("runtime.scene_draw_budget");
                const auto& color = material.base_color_;
                result.draws_.push_back({upload.meshes_.at(mesh_index).Handle(),
                                         Matrix(transform),
                                         {color.red_, color.green_, color.blue_, color.alpha_},
                                         material.double_sided_});
                auto& draw = result.draws_.back();
                draw.unlit_ = material.unlit_;
                draw.metallic_ = material.metallic_;
                draw.roughness_ = material.roughness_;
                draw.emissive_ = {static_cast<float>(material.emissive_.x_),
                                  static_cast<float>(material.emissive_.y_),
                                  static_cast<float>(material.emissive_.z_)};
                if (!material.unlit_) draw.normal_ = Matrix(scene::NormalTransform(transform));
            }
        }
    }
    // Opaque objects write depth first. Translucent object origins sort back to
    // front; intersecting transparent meshes require a later transparency pass.
    const auto depth = [&](const render::MeshDraw& draw) {
        return scene::TransformPoint(view, {draw.model_[12], draw.model_[13], draw.model_[14]}).z_;
    };
    std::stable_sort(result.draws_.begin(), result.draws_.end(), [&](const auto& a, const auto& b) {
        const bool opaque_a = a.color_[3] >= 1, opaque_b = b.color_[3] >= 1;
        if (opaque_a != opaque_b) return opaque_a;
        return !opaque_a && depth(a) < depth(b);
    });
    return result;
}
}  // namespace rhythm::runtime::detail
