#include "scene_pass.h"

#include <algorithm>
#include <cmath>
#include <numeric>
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
scene::Vector3 MeshCenter(const scene::Mesh& mesh) {
    if (mesh.vertices_.empty()) throw std::invalid_argument("runtime.empty_mesh");
    scene::Vector3 minimum{mesh.vertices_[0].x_, mesh.vertices_[0].y_, mesh.vertices_[0].z_};
    auto maximum = minimum;
    for (const auto& vertex : mesh.vertices_) {
        minimum.x_ = std::min(minimum.x_, double(vertex.x_));
        minimum.y_ = std::min(minimum.y_, double(vertex.y_));
        minimum.z_ = std::min(minimum.z_, double(vertex.z_));
        maximum.x_ = std::max(maximum.x_, double(vertex.x_));
        maximum.y_ = std::max(maximum.y_, double(vertex.y_));
        maximum.z_ = std::max(maximum.z_, double(vertex.z_));
    }
    return {(minimum.x_ + maximum.x_) * 0.5, (minimum.y_ + maximum.y_) * 0.5,
            (minimum.z_ + maximum.z_) * 0.5};
}
}  // namespace
ScenePass::PoseMatrices ScenePass::PreparePose(const scene::Model& model,
                                               const scene::AnimationPose& pose) {
    PoseMatrices result;
    result.worlds_ = scene::WorldTransforms(model, pose);
    for (const auto& [node, palette] : scene::SkinPalettes(model, result.worlds_)) {
        auto& converted = result.skins_[node];
        for (const auto& matrix : palette) converted.push_back(Matrix(matrix));
    }
    return result;
}
render::SceneDrawList ScenePass::Build(const scene::Scene& scene, const scene::Camera& camera,
                                       render::Extent extent, render::Renderer& renderer,
                                       std::span<const NodeOutput> outputs) {
    if (!extent.width_ || !extent.height_ ||
        scene.instances_.size() > graph::kMaximumSceneInstances ||
        scene.lights_.size() + scene.positional_lights_.size() > 4 || !renderer.SupportsScenes())
        throw std::invalid_argument("runtime.scene");
    const auto view = scene::View(camera);
    std::map<graph::NodeId, render::SurfaceProgramInput> surfaces;
    for (const auto& output : outputs)
        if (output.surface_program_) surfaces.emplace(output.node_, *output.surface_program_);
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
    std::size_t palette_matrices = 0;
    const auto count_palette = [&](const scene::Model& model) {
        for (const auto& node : model.nodes_) {
            if (!node.skin_) continue;
            const auto count = model.skins_.at(*node.skin_).joints_.size();
            if (count > 65536 - palette_matrices)
                throw std::length_error("runtime.animation_budget");
            palette_matrices += count;
        }
    };
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
            !scene::ValidAffine(instance.transform_) || !std::isfinite(instance.sorting_offset_) ||
            std::abs(instance.sorting_offset_) > 10000)
            throw std::invalid_argument("runtime.geometry");
        const auto& geometry = *instance.geometry_;
        if (geometry.pose_ && active_poses.emplace(geometry.id_, geometry.revision_).second) {
            count_palette(*geometry.model_);
            if (geometry.model_->nodes_.size() > 65536 - pose_nodes)
                throw std::length_error("runtime.animation_budget");
            pose_nodes += geometry.model_->nodes_.size();
        }
        if (active.emplace(geometry.upload_id_ ? geometry.upload_id_ : geometry.id_,
                           geometry.upload_id_ ? geometry.upload_revision_ : geometry.revision_,
                           needs_tangents(instance))
                    .second)
            count_palette(*geometry.model_);
    }
    std::erase_if(uploads_, [&](const auto& item) { return !active.contains(item.first); });
    std::erase_if(poses_, [&](const auto& item) { return !active_poses.contains(item.first); });
    std::uint64_t index_count = 0;
    std::size_t draw_bones = 0;
    std::vector<double> draw_depths;
    std::vector<std::int32_t> draw_priorities;
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
            upload.rest_ = PreparePose(*geometry.model_);
            std::size_t tangent_vertices = 0;
            for (auto mesh : geometry.model_->meshes_) {
                upload.mesh_centers_.push_back(MeshCenter(mesh));
                if (std::get<2>(key) && !mesh.has_tangents_) scene::GenerateTangents(mesh);
                tangent_vertices += mesh.vertices_.size();
                if (tangent_vertices > 250000) throw std::length_error("runtime.tangent_budget");
                std::vector<render::MeshVertex> vertices;
                vertices.reserve(mesh.vertices_.size());
                for (const auto& v : mesh.vertices_)
                    vertices.push_back({v.x_, v.y_, v.z_, v.normal_x_, v.normal_y_, v.normal_z_,
                                        v.u_, v.v_, v.tangent_});
                std::vector<render::SkinWeights> weights;
                weights.reserve(mesh.skin_.size());
                for (const auto& vertex : mesh.skin_)
                    weights.push_back({vertex.joints_, vertex.weights_});
                std::vector<render::MorphTarget> morphs(mesh.morphs_.size());
                for (std::size_t i = 0; i < morphs.size(); ++i) {
                    auto& deltas = morphs[i].deltas_;
                    deltas.reserve(mesh.vertices_.size());
                    for (const auto& delta : mesh.morphs_[i].deltas_)
                        deltas.push_back({delta.position_, delta.normal_, delta.tangent_});
                }
                upload.meshes_.push_back(
                        renderer.CreateMesh(vertices, mesh.indices_, weights, morphs));
            }
            uploads_.emplace(key, std::move(upload));
        }
        auto& upload = uploads_.at(key);
        const PoseKey pose_key{geometry.id_, geometry.revision_};
        if (geometry.pose_ && !poses_.contains(pose_key))
            poses_.emplace(pose_key, PreparePose(*upload.model_, *geometry.pose_));
        const auto& matrices = geometry.pose_ ? poses_.at(pose_key) : upload.rest_;
        for (const auto& node : upload.model_->nodes_) {
            const auto& world = matrices.worlds_.at(node.id_);
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
                                         material.double_sided_,
                                         material.alpha_depth_prepass_});
                const auto center =
                        scene::TransformPoint(transform, upload.mesh_centers_[mesh_index]);
                if (camera.kind_ == scene::ProjectionKind::kOrthographic) {
                    draw_depths.push_back(-scene::TransformPoint(view, center).z_ -
                                          instance.sorting_offset_);
                } else {
                    draw_depths.push_back(std::hypot(center.x_ - camera.eye_.x_,
                                                     center.y_ - camera.eye_.y_,
                                                     center.z_ - camera.eye_.z_) -
                                          instance.sorting_offset_);
                }
                draw_priorities.push_back(material.render_priority_);
                auto& draw = result.draws_.back();
                if (!mesh.morphs_.empty()) {
                    const auto& pose = geometry.pose_ ? *geometry.pose_ : upload.model_->rest_pose_;
                    const auto& weights = pose.at(node.id_).weights_;
                    for (std::size_t i = 0; i < mesh.morphs_.size(); ++i)
                        draw.morph_weights_[i] = static_cast<float>(weights[i]);
                }
                if (node.skin_) {
                    const auto& bones = matrices.skins_.at(node.id_);
                    if (bones.size() > 65536 - draw_bones)
                        throw std::length_error("runtime.animation_budget");
                    draw_bones += bones.size();
                    draw.bones_ = bones;
                }
                for (const auto& modifier : geometry.deformations_)
                    draw.deformations_.push_back(
                            {float(modifier.twist_),
                             float(modifier.taper_),
                             modifier.axis_,
                             {float(modifier.pivot_.x_), float(modifier.pivot_.y_),
                              float(modifier.pivot_.z_)}});
                draw.unlit_ = material.unlit_;
                if (material.surface_node_) {
                    const auto found = surfaces.find(material.surface_node_);
                    if (found == surfaces.end() || !renderer.IsValid(found->second.program_))
                        throw std::invalid_argument("runtime.material_surface");
                    draw.surface_program_ = found->second;
                }
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
                if (!material.unlit_ || material.surface_node_)
                    draw.normal_ = Matrix(scene::NormalTransform(transform));
            }
        }
    }
    // Match Godot's opaque-first and transparent back-to-front object ordering.
    // A mesh AABB center remains meaningful when its local geometry is offset
    // from the instance origin. Intersecting geometry still needs a material
    // depth mode or a separately validated transparency technique.
    std::vector<std::size_t> order(result.draws_.size());
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        const bool opaque_a =
                result.draws_[a].color_[3] >= 1 && !result.draws_[a].alpha_depth_prepass_;
        const bool opaque_b =
                result.draws_[b].color_[3] >= 1 && !result.draws_[b].alpha_depth_prepass_;
        if (opaque_a != opaque_b) return opaque_a;
        if (opaque_a) return false;
        if (draw_priorities[a] != draw_priorities[b])
            return draw_priorities[a] < draw_priorities[b];
        return draw_depths[a] > draw_depths[b];
    });
    std::vector<render::MeshDraw> sorted;
    sorted.reserve(result.draws_.size());
    for (const auto index : order) sorted.push_back(std::move(result.draws_[index]));
    result.draws_ = std::move(sorted);
    environment_.Apply(scene.environment_, outputs, result, renderer);
    shadow_.Apply(scene, result, renderer);
    return result;
}
}  // namespace rhythm::runtime::detail
