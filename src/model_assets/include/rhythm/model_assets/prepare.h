#pragma once

#include <stop_token>

#include "rhythm/project/package.h"
#include "rhythm/scene/resources.h"

namespace rhythm::model_assets {
// One synchronous preparation worker owns a cache. Only the last successful
// catalog is retained; consumers may independently hold immutable older results.
class Cache final {
   public:
    std::shared_ptr<const scene::Resources> Prepare(const graph::ExecutionPlan& plan,
                                                    std::span<const project::PackagedAsset> assets,
                                                    std::stop_token stop = {});

   private:
    std::shared_ptr<const scene::Resources> previous_{};
};
// Blocking preparation: callers use a bounded worker or an explicit cold load.
// Hash-verified embedded GLB bytes become immutable CPU resources; no GPU work.
std::shared_ptr<const scene::Resources> Prepare(const graph::ExecutionPlan& plan,
                                                std::span<const project::PackagedAsset> assets,
                                                std::stop_token stop = {});
// Reuses an existing catalog for parameter-only graph edits, without parsing/I/O.
bool Covers(const graph::ExecutionPlan& plan, const scene::Resources& resources);
}  // namespace rhythm::model_assets
