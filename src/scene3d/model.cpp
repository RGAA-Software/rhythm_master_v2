#include "rhythm/scene/model.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace rhythm::scene {
namespace {
bool Bounded(double value, double maximum = 1e6) {
    return std::isfinite(value) && std::abs(value) <= maximum;
}
void Require(bool condition) {
    if (!condition) throw std::invalid_argument("scene.model");
}
}  // namespace
std::map<NodeId, WorldNode> WorldTransforms(const Model& model, const AnimationPose& pose) {
    Require(model.nodes_.size() <= 2048);
    Validate(pose);
    std::map<NodeId, std::size_t> lookup;
    for (std::size_t i = 0; i < model.nodes_.size(); ++i)
        Require(model.nodes_[i].id_ && lookup.emplace(model.nodes_[i].id_, i).second &&
                ValidAffine(model.nodes_[i].local_));
    for (const auto& [id, value] : pose) {
        (void)value;
        Require(lookup.contains(id));
    }
    std::map<NodeId, WorldNode> worlds;
    std::map<NodeId, std::size_t> depths;
    for (const auto& node : model.nodes_) {
        std::vector<NodeId> path;
        std::set<NodeId> visited;
        std::optional<NodeId> cursor = node.id_;
        while (cursor && !worlds.contains(*cursor)) {
            Require(lookup.contains(*cursor) && visited.insert(*cursor).second && path.size() < 64);
            path.push_back(*cursor);
            cursor = model.nodes_[lookup.at(*cursor)].parent_;
        }
        while (!path.empty()) {
            const auto id = path.back();
            path.pop_back();
            const auto& source = model.nodes_[lookup.at(id)];
            const auto parent = source.parent_ ? worlds.at(*source.parent_) : WorldNode{};
            const auto depth = source.parent_ ? depths.at(*source.parent_) + 1 : 1;
            Require(depth <= 64);
            auto local = source.local_;
            if (const auto found = pose.find(id); found != pose.end()) {
                const auto& animated = found->second;
                local = Compose(animated.translation_, animated.rotation_, animated.scale_);
            }
            WorldNode value{Multiply(parent.transform_, local), parent.visible_ && source.visible_};
            Require(ValidAffine(value.transform_));
            worlds.emplace(id, value);
            depths.emplace(id, depth);
        }
    }
    return worlds;
}
void Validate(const Model& model) {
    Require(!model.nodes_.empty() && model.nodes_.size() <= 2048 && !model.materials_.empty() &&
            model.materials_.size() <= 128 && model.meshes_.size() <= 512);
    ValidateAnimations(model);
    ValidateSkins(model);
    Require(model.images_.size() <= 192);
    std::size_t image_bytes = 0;
    for (const auto& image : model.images_) {
        Require(image.width_ > 0 && image.height_ > 0 && image.width_ <= 4096 &&
                image.height_ <= 4096 && std::size_t(image.width_) * image.height_ <= 2073600 &&
                image.rgba_.size() == std::size_t(image.width_) * image.height_ * 4 &&
                image.rgba_.size() <= kMaximumModelImageBytes - image_bytes);
        image_bytes += image.rgba_.size();
    }
    for (const auto& material : model.materials_) {
        for (const auto index : material.textures_.images_)
            Require(!index || *index < model.images_.size());
        const auto& c = material.base_color_;
        for (const auto value :
             {c.red_, c.green_, c.blue_, c.alpha_, material.metallic_, material.roughness_})
            Require(Bounded(value, 1) && value >= 0);
        Require(Bounded(material.emissive_.x_, 1000) && material.emissive_.x_ >= 0 &&
                Bounded(material.emissive_.y_, 1000) && material.emissive_.y_ >= 0 &&
                Bounded(material.emissive_.z_, 1000) && material.emissive_.z_ >= 0);
    }
    std::size_t vertices = 0, indices = 0, references = 0;
    for (const auto& mesh : model.meshes_) {
        vertices += mesh.vertices_.size();
        indices += mesh.indices_.size();
        Require(vertices <= 250000 && indices <= 750000 && !mesh.vertices_.empty() &&
                !mesh.indices_.empty() && mesh.indices_.size() % 3 == 0 &&
                mesh.material_ < model.materials_.size());
        for (const auto& v : mesh.vertices_) {
            Require(Bounded(v.x_) && Bounded(v.y_) && Bounded(v.z_) && Bounded(v.u_) &&
                    Bounded(v.v_));
            const auto length = std::hypot(v.normal_x_, v.normal_y_, v.normal_z_);
            Require(std::isfinite(length) && length > 0.99f && length < 1.01f);
            if (mesh.has_tangents_) {
                const auto& t = v.tangent_;
                const auto tangent_length = std::hypot(t[0], t[1], t[2]);
                Require(std::isfinite(tangent_length) && tangent_length > 0.99f &&
                        tangent_length < 1.01f && std::abs(t[3]) == 1 &&
                        std::abs(t[0] * v.normal_x_ + t[1] * v.normal_y_ + t[2] * v.normal_z_) <
                                0.01f);
            }
        }
        for (const auto index : mesh.indices_) Require(index < mesh.vertices_.size());
    }
    for (const auto& node : model.nodes_) {
        Require(node.name_.size() <= 1024);
        references += node.meshes_.size();
        Require(references <= 4096);
        for (const auto mesh : node.meshes_) Require(mesh < model.meshes_.size());
    }
    (void)SkinPalettes(model, WorldTransforms(model));
}
void GenerateNormals(Mesh& mesh) {
    mesh.has_tangents_ = false;
    Require(mesh.vertices_.size() <= 250000 && mesh.indices_.size() <= 750000 &&
            mesh.indices_.size() % 3 == 0);
    std::vector<Vector3> normals(mesh.vertices_.size());
    for (std::size_t i = 0; i < mesh.indices_.size(); i += 3) {
        const auto a = mesh.indices_[i], b = mesh.indices_[i + 1], c = mesh.indices_[i + 2];
        Require(a < mesh.vertices_.size() && b < mesh.vertices_.size() &&
                c < mesh.vertices_.size());
        const auto& x = mesh.vertices_[a];
        const auto& y = mesh.vertices_[b];
        const auto& z = mesh.vertices_[c];
        const auto normal = Cross({double(y.x_) - x.x_, double(y.y_) - x.y_, double(y.z_) - x.z_},
                                  {double(z.x_) - x.x_, double(z.y_) - x.y_, double(z.z_) - x.z_});
        for (const auto index : {a, b, c}) {
            normals[index].x_ += normal.x_;
            normals[index].y_ += normal.y_;
            normals[index].z_ += normal.z_;
        }
    }
    for (std::size_t i = 0; i < normals.size(); ++i) {
        const auto normal =
                Dot(normals[i], normals[i]) < 1e-20 ? Vector3{0, 0, 1} : Normalize(normals[i]);
        mesh.vertices_[i].normal_x_ = static_cast<float>(normal.x_);
        mesh.vertices_[i].normal_y_ = static_cast<float>(normal.y_);
        mesh.vertices_[i].normal_z_ = static_cast<float>(normal.z_);
    }
}
Model Cube() {
    Model model;
    model.materials_.emplace_back();
    Mesh mesh;
    // Separate face vertices preserve hard normals and UV seams.
    const std::array<std::array<Vector3, 4>, 6> faces{
            {{{{-.5, -.5, .5}, {.5, -.5, .5}, {.5, .5, .5}, {-.5, .5, .5}}},
             {{{.5, -.5, -.5}, {-.5, -.5, -.5}, {-.5, .5, -.5}, {.5, .5, -.5}}},
             {{{.5, -.5, .5}, {.5, -.5, -.5}, {.5, .5, -.5}, {.5, .5, .5}}},
             {{{-.5, -.5, -.5}, {-.5, -.5, .5}, {-.5, .5, .5}, {-.5, .5, -.5}}},
             {{{-.5, .5, .5}, {.5, .5, .5}, {.5, .5, -.5}, {-.5, .5, -.5}}},
             {{{-.5, -.5, -.5}, {.5, -.5, -.5}, {.5, -.5, .5}, {-.5, -.5, .5}}}}};
    for (const auto& face : faces) {
        const auto base = static_cast<std::uint32_t>(mesh.vertices_.size());
        for (std::size_t i = 0; i < 4; ++i)
            mesh.vertices_.push_back({static_cast<float>(face[i].x_),
                                      static_cast<float>(face[i].y_),
                                      static_cast<float>(face[i].z_), 0, 0, 1,
                                      i == 1 || i == 2 ? 1.0f : 0.0f, i < 2 ? 1.0f : 0.0f});
        for (const auto index : {0U, 1U, 2U, 0U, 2U, 3U}) mesh.indices_.push_back(base + index);
    }
    GenerateNormals(mesh);
    model.meshes_.push_back(std::move(mesh));
    model.nodes_.push_back({1, {}, {}, {0}, true, "Cube"});
    return model;
}
}  // namespace rhythm::scene
