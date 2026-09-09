#pragma once

#include "text_assets.h"

namespace rhythm::prepared_assets::detail {
// Worker-confined, last-successful-generation cache. Immutable models/video are
// shared; image values are copied within the existing aggregate image budget.
class PreparationCache final {
   public:
    std::shared_ptr<const Resources> Prepare(const graph::ExecutionPlan& plan,
                                             std::span<const project::PackagedAsset> assets,
                                             std::stop_token stop = {});
    const TextCache& Text() const { return text_; }

   private:
    model_assets::Cache models_{};
    TextCache text_{};
    std::shared_ptr<const Resources> previous_ = std::make_shared<const Resources>();
};
}  // namespace rhythm::prepared_assets::detail
