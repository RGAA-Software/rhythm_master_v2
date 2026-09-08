#include <set>
#include <stdexcept>

#include "rhythm/scene/model.h"

namespace rhythm::scene {
void ValidateAnimations(const Model& model) {
    const auto require = [](bool condition) {
        if (!condition) throw std::invalid_argument("scene.model_animation");
    };
    Validate(model.rest_pose_);
    require(model.animations_.size() <= 64);
    std::set<NodeId> nodes;
    for (const auto& node : model.nodes_) nodes.insert(node.id_);
    for (const auto& [id, pose] : model.rest_pose_) {
        (void)pose;
        require(nodes.contains(id));
    }
    std::size_t tracks = 0, values = 0;
    for (const auto& clip : model.animations_) {
        require(clip.Tracks().size() <= 8192 - tracks);
        tracks += clip.Tracks().size();
        for (const auto& track : clip.Tracks()) {
            require(model.rest_pose_.contains(track.node_) &&
                    track.values_.size() <= 262144 - values);
            values += track.values_.size();
        }
    }
}
}  // namespace rhythm::scene
