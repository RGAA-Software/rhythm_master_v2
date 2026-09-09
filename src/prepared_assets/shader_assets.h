#pragma once

#include "rhythm/prepared_assets/prepare.h"

namespace rhythm::prepared_assets::detail {
std::shared_ptr<const image_shader::Resources> PrepareShaders(
        const graph::ExecutionPlan& plan, std::span<const project::PackagedAsset> assets,
        std::stop_token stop, const image_shader::Resources& previous);
std::shared_ptr<const surface_shader::Resources> PrepareSurfaces(
        const graph::ExecutionPlan& plan, std::span<const project::PackagedAsset> assets,
        std::stop_token stop, const surface_shader::Resources& previous);
}  // namespace rhythm::prepared_assets::detail
