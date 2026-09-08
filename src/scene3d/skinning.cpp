#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

#include "rhythm/scene/model.h"

namespace rhythm::scene {
namespace {
void Require(bool condition) {
    if (!condition) throw std::invalid_argument("scene.skin");
}
}  // namespace
void ValidateSkins(const Model& model) {
    Require(model.skins_.size() <= 64 && model.nodes_.size() <= 2048 &&
            model.meshes_.size() <= 512);
    std::set<NodeId> nodes;
    for (const auto& node : model.nodes_) nodes.insert(node.id_);
    for (const auto& skin : model.skins_) {
        Require(!skin.joints_.empty() && skin.joints_.size() <= kMaximumModelSkinBones &&
                skin.joints_.size() == skin.inverse_bind_.size());
        std::set<NodeId> unique;
        for (std::size_t i = 0; i < skin.joints_.size(); ++i) {
            Require(nodes.contains(skin.joints_[i]) && unique.insert(skin.joints_[i]).second &&
                    ValidAffine(skin.inverse_bind_[i]));
            (void)InverseAffine(skin.inverse_bind_[i]);
        }
    }
    std::vector<std::uint8_t> required_bones;
    for (const auto& mesh : model.meshes_) {
        Require(mesh.skin_.empty() || mesh.skin_.size() == mesh.vertices_.size());
        Require(mesh.skin_.size() <= 250000);
        std::uint8_t bones = 0;
        for (const auto& vertex : mesh.skin_) {
            float sum = 0;
            for (std::size_t i = 0; i < 4; ++i) {
                Require(vertex.joints_[i] < kMaximumModelSkinBones &&
                        std::isfinite(vertex.weights_[i]) && vertex.weights_[i] >= 0 &&
                        vertex.weights_[i] <= 1);
                sum += vertex.weights_[i];
                bones = std::max(bones, static_cast<std::uint8_t>(vertex.joints_[i] + 1));
            }
            Require(std::abs(sum - 1) < 1e-4f);
        }
        required_bones.push_back(bones);
    }
    std::size_t references = 0;
    for (const auto& node : model.nodes_) {
        Require(node.meshes_.size() <= 4096 - references);
        references += node.meshes_.size();
        Require(!node.skin_ || (*node.skin_ < model.skins_.size() && !node.meshes_.empty()));
        for (const auto mesh_index : node.meshes_) {
            Require(mesh_index < model.meshes_.size());
            const auto& mesh = model.meshes_[mesh_index];
            Require(node.skin_.has_value() == !mesh.skin_.empty());
            if (!node.skin_) continue;
            Require(required_bones[mesh_index] <= model.skins_[*node.skin_].joints_.size());
        }
    }
}
std::map<NodeId, std::vector<Matrix>> SkinPalettes(const Model& model,
                                                   const std::map<NodeId, WorldNode>& worlds) {
    Require(model.nodes_.size() <= 2048 && model.skins_.size() <= 64);
    std::map<NodeId, std::vector<Matrix>> result;
    std::size_t matrices = 0;
    for (const auto& node : model.nodes_) {
        if (!node.skin_) continue;
        Require(*node.skin_ < model.skins_.size() && worlds.contains(node.id_));
        const auto& skin = model.skins_.at(*node.skin_);
        Require(skin.joints_.size() <= kMaximumModelSkinBones &&
                skin.joints_.size() == skin.inverse_bind_.size() &&
                skin.joints_.size() <= 65536 - matrices);
        matrices += skin.joints_.size();
        const auto inverse = InverseAffine(worlds.at(node.id_).transform_);
        auto& palette = result[node.id_];
        for (std::size_t i = 0; i < skin.joints_.size(); ++i) {
            Require(worlds.contains(skin.joints_[i]));
            const auto matrix = Multiply(Multiply(inverse, worlds.at(skin.joints_[i]).transform_),
                                         skin.inverse_bind_[i]);
            Require(ValidAffine(matrix));
            palette.push_back(matrix);
        }
    }
    return result;
}
}  // namespace rhythm::scene
