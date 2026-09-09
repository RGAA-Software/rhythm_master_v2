#pragma once

#include "rhythm/prepared_assets/prepare.h"

namespace rhythm::prepared_assets::detail {
std::vector<VideoSource> PrepareVideos(const graph::ExecutionPlan& plan,
                                       std::span<const project::PackagedAsset> assets,
                                       std::stop_token stop, std::span<const VideoSource> previous);
}  // namespace rhythm::prepared_assets::detail
