#include "rhythm/scene/resources.h"

#include <algorithm>
#include <stdexcept>

namespace rhythm::scene {
ModelResource DescribeModel(assets::AssetId id, Model model) {
    if (!assets::ValidId(id)) throw std::invalid_argument("scene.asset_id");
    Validate(model);
    ModelResource resource;
    resource.id_ = std::move(id);
    for (const auto& mesh : model.meshes_) {
        resource.vertices_ += mesh.vertices_.size();
        resource.indices_ += mesh.indices_.size();
    }
    const auto worlds = WorldTransforms(model);
    for (const auto& node : model.nodes_) {
        if (!worlds.at(node.id_).visible_) continue;
        for (const auto index : node.meshes_) {
            resource.draw_indices_ += model.meshes_.at(index).indices_.size();
            ++resource.draws_;
        }
    }
    if (resource.draw_indices_ > 3000000 || resource.draws_ > 4096)
        throw std::length_error("scene.asset_draw_budget");
    resource.model_ = std::make_shared<const Model>(std::move(model));
    return resource;
}
const ModelResource& FindModel(const Resources& resources, const assets::AssetId& id) {
    if (!assets::ValidId(id) || resources.models_.size() > 64)
        throw std::invalid_argument("scene.resources");
    const auto found = std::find_if(resources.models_.begin(), resources.models_.end(),
                                    [&](const auto& resource) { return resource.id_ == id; });
    if (found == resources.models_.end() || !found->model_)
        throw std::invalid_argument("scene.asset_missing");
    return *found;
}
}  // namespace rhythm::scene
