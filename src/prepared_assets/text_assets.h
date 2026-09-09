#pragma once

#include "rhythm/prepared_assets/prepare.h"

namespace rhythm::prepared_assets::detail {
void PrepareText(const graph::ExecutionPlan& plan, std::span<const project::PackagedAsset> assets,
                 assets::Images& images, std::size_t model_image_bytes, std::stop_token stop);
}  // namespace rhythm::prepared_assets::detail
