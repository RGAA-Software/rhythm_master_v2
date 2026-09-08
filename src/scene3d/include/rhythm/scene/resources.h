#pragma once

#include <memory>

#include "rhythm/assets/types.h"
#include "rhythm/scene/model.h"

namespace rhythm::scene {
struct ModelResource {
    assets::AssetId id_{};
    std::shared_ptr<const Model> model_{};
    std::uint64_t vertices_ = 0;
    std::uint64_t indices_ = 0;
    std::uint64_t draw_indices_ = 0;
    std::uint64_t draws_ = 0;
    std::uint64_t image_bytes_ = 0;
};
// Prepared off the render thread, published as shared immutable values. No file
// paths, parsers, GPU objects or asset bytes survive in the resource catalog.
struct Resources {
    std::vector<ModelResource> models_{};
};
ModelResource DescribeModel(assets::AssetId id, Model model);
const ModelResource& FindModel(const Resources& resources, const assets::AssetId& id);
}  // namespace rhythm::scene
