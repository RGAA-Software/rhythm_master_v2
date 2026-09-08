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
                                       render::Extent extent, render::Renderer& renderer,
                                       std::span<const NodeOutput> outputs) {
    if (!extent.width_ || !extent.height_ ||
        scene.instances_.size() > graph::kMaximumSceneInstances ||
        scene.lights_.size() + scene.positional_lights_.size() > 4 || !renderer.SupportsScenes())
        throw std::invalid_argument("runtime.scene");
    const auto view = scene::View(camera);
    std::map<graph::NodeId, render::TextureHandle> textures;
    for (const auto& output : outputs)
        if (output.texture_.device_) textures.emplace(output.node_, output.texture_);
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
    for (const auto& light : scene.positional_lights_)
        result.positional_lights_.push_back(
                {{float(light.position_.x_), float(light.position_.y_), float(light.position_.z_)},
                 {float(light.radiance_.x_), float(light.radiance_.y_), float(light.radiance_.z_)},
                 {float(light.direction_.x_), float(light.direction_.y_),
                  float(light.direction_.z_)},
                 float(light.range_),
                 float(light.decay_),
                 light.spot_,
                 float(light.cone_angle_),
                 float(light.cone_decay_)});
    std::set<Key> active;
    std::set<PoseKey> active_poses;
    std::size_t pose_nodes = 0;
    const auto needs_tangents = [](const scene::Instance& instance) {
        if (instance.material_)
            return instance.material_->textures_.nodes_[1] != 0 ||
                   instance.material_->textures_.images_[1].has_value();
        return std::any_of(instance.geometry_->model_->materials_.begin(),
                           instance.geometry_->model_->materials_.end(), [](const auto& material) {
                               return material.textures_.nodes_[1] != 0 ||
                                      material.textures_.images_[1].has_value();
                           });
    };
    for (const auto& instance : scene.instances_) {
        if (!instance.geometry_ || !instance.geometry_->model_ || !instance.geometry_->id_ ||
            !scene::ValidAffine(instance.transform_))
            throw std::invalid_argument("runtime.geometry");
        const auto& geometry = *instance.geometry_;
        if (geometry.pose_ && active_poses.emplace(geometry.id_, geometry.revision_).second) {
            if (geometry.model_->nodes_.size() > 65536 - pose_nodes)
                throw std::length_error("runtime.animation_budget");
            pose_nodes += geometry.model_->nodes_.size();
        }
        active.emplace(geometry.upload_id_ ? geometry.upload_id_ : geometry.id_,
                       geometry.upload_id_ ? geometry.upload_revision_ : geometry.revision_,
                       needs_tangents(instance));
    }
    std::erase_if(uploads_, [&](const auto& item) { return !active.contains(item.first); });
    std::erase_if(poses_, [&](const auto& item) { return !active_poses.contains(item.first); });
    std::uint64_t index_count = 0;
    for (const auto& instance : scene.instances_) {
        const auto& geometry = *instance.geometry_;
        const Key key{geometry.upload_id_ ? geometry.upload_id_ : geometry.id_,
                      geometry.upload_id_ ? geometry.upload_revision_ : geometry.revision_,
                      needs_tangents(instance)};
        if (!uploads_.contains(key)) {
            scene::Validate(*geometry.model_);
            Uploaded upload;
            upload.model_ = geometry.model_;
            upload.images_.resize(geometry.model_->images_.size());
            upload.worlds_ = scene::WorldTransforms(*geometry.model_);
            std::size_t tangent_vertices = 0;
            for (auto mesh : geometry.model_->meshes_) {
                if (std::get<2>(key) && !mesh.has_tangents_) scene::GenerateTangents(mesh);
                tangent_vertices += mesh.vertices_.size();
                if (tangent_vertices > 250000) throw std::length_error("runtime.tangent_budget");
                std::vector<render::MeshVertex> vertices;
                vertices.reserve(mesh.vertices_.size());
                for (const auto& v : mesh.vertices_)
                    vertices.push_back({v.x_, v.y_, v.z_, v.normal_x_, v.normal_y_, v.normal_z_,
                                        v.u_, v.v_, v.tangent_});
                upload.meshes_.push_back(renderer.CreateMesh(vertices, mesh.indices_));
            }
            uploads_.emplace(key, std::move(upload));
        }
        auto& upload = uploads_.at(key);
        const PoseKey pose_key{geometry.id_, geometry.revision_};
        if (geometry.pose_ && !poses_.contains(pose_key))
            poses_.emplace(pose_key, scene::WorldTransforms(*upload.model_, *geometry.pose_));
        const auto& worlds = geometry.pose_ ? poses_.at(pose_key) : upload.worlds_;
        for (const auto& node : upload.model_->nodes_) {
            const auto& world = worlds.at(node.id_);
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
                for (const auto& modifier : geometry.deformations_)
                    draw.deformations_.push_back(
                            {float(modifier.twist_),
                             float(modifier.taper_),
                             modifier.axis_,
                             {float(modifier.pivot_.x_), float(modifier.pivot_.y_),
                              float(modifier.pivot_.z_)}});
                draw.unlit_ = material.unlit_;
                draw.textures_.color_srgb_ = material.textures_.color_srgb_;
                draw.textures_.normal_scale_ = material.textures_.normal_scale_;
                draw.textures_.uv_transform_ = material.textures_.uv_transform_;
                for (std::size_t slot = 0; slot < 4; ++slot) {
                    const auto texture_node = material.textures_.nodes_[slot];
                    if (!texture_node) {
                        if (const auto image = material.textures_.images_[slot]) {
                            const auto& pixels = upload.model_->images_.at(*image);
                            auto& texture = upload.images_.at(*image);
                            if (!renderer.IsValid(texture.Handle()))
                                texture = renderer.CreateTexture({pixels.width_, pixels.height_},
                                                                 pixels.rgba_);
                            draw.textures_.slots_[slot] = texture.Handle();
                        }
                        continue;
                    }
                    const auto found = textures.find(texture_node);
                    if (found == textures.end() || !renderer.IsValid(found->second))
                        throw std::invalid_argument("runtime.material_texture");
                    draw.textures_.slots_[slot] = found->second;
                }
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
    environment_.Apply(scene.environment_, outputs, result, renderer);
    shadow_.Apply(scene, result, renderer);
    return result;
}
}  // namespace rhythm::runtime::detail
