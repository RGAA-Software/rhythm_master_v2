#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "rhythm/scene/model.h"

namespace rhythm::scene {
void ValidateMorphs(const Model& model) {
    const auto require = [](bool condition) {
        if (!condition) throw std::invalid_argument("scene.morph");
    };
    require(model.meshes_.size() <= 512 && model.nodes_.size() <= 2048);
    std::size_t deltas = 0;
    for (const auto& mesh : model.meshes_) {
        require(mesh.morphs_.size() <= 4);
        for (const auto& target : mesh.morphs_) {
            require(target.deltas_.size() == mesh.vertices_.size() &&
                    target.deltas_.size() <= 1000000 - deltas);
            deltas += target.deltas_.size();
            for (const auto& delta : target.deltas_)
                for (const auto& values : {delta.position_, delta.normal_, delta.tangent_})
                    for (const auto value : values)
                        require(std::isfinite(value) && std::abs(value) <= 1e6f);
        }
    }
    std::map<NodeId, std::size_t> counts;
    std::size_t references = 0;
    for (const auto& node : model.nodes_) {
        require(node.meshes_.size() <= 4096 - references);
        references += node.meshes_.size();
        auto& count = counts[node.id_];
        for (const auto index : node.meshes_) {
            require(index < model.meshes_.size());
            count = std::max(count, model.meshes_[index].morphs_.size());
        }
        if (count) require(model.rest_pose_.contains(node.id_));
    }
    for (const auto& clip : model.animations_)
        for (const auto& track : clip.Tracks())
            if (track.property_ == AnimationProperty::kWeights)
                require(counts.contains(track.node_) &&
                        counts.at(track.node_) == track.components_);
}
}  // namespace rhythm::scene
