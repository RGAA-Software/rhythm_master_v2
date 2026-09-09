#pragma once

#include <span>

#include "rhythm/graph/document.h"
#include "rhythm/scene/picking.h"

namespace rhythm::runtime {
struct NodeOutput;
}

namespace rhythm::studio {
struct SceneSelection {
    std::optional<graph::NodeId> selected_{};
    std::optional<scene::PickHit> hit_{};
    std::string error_{};
};
// Root direct scene output only. Caller guarantees matching accepted graph/frame
// generation. No reconstruction of animated instance transforms from properties.
SceneSelection PickSceneOutput(const graph::Document& document,
                               std::span<const runtime::NodeOutput> outputs, double aspect,
                               double x, double y);
}  // namespace rhythm::studio
