#pragma once

#include "rhythm/prepared_assets/prepare.h"

namespace rhythm::prepared_assets::detail {
std::vector<VideoSource> PrepareVideos(const graph::ExecutionPlan& plan,
                                       std::span<const project::PackagedAsset> assets,
                                       std::stop_token stop);
}  // namespace rhythm::prepared_assets::detail
