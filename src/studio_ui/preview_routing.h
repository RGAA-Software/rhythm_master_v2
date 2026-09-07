#pragma once

#include "graph_canvas.h"
#include "rhythm/editor/compiler_worker.h"

namespace rhythm::studio {
struct PreviewRequest {
    std::vector<graph::NodeId> roots_{};
    editor::ScopedViewers scoped_{};
};
// UI-thread routing for a single shared preview budget. Compiled instance IDs
// become visible only when their matching plan/resources are committed.
class PreviewRouting final {
   public:
    PreviewRequest Prepare(std::vector<graph::NodeId> roots, editor::ScopedViewers scoped);
    void Stage(const editor::Compilation& compilation);
    void Commit();
    bool TakeInvalidation();
    std::span<const graph::NodeId> ActiveNodes() const { return active_nodes_; }
    CanvasPreviews Scoped(const CanvasPreviews& previews) const;

   private:
    std::vector<graph::NodeId> active_nodes_{};
    std::vector<graph::NodeId> pending_nodes_{};
    std::map<graph::NodeId, graph::NodeId> scoped_nodes_{};
    std::map<graph::NodeId, graph::NodeId> pending_scoped_nodes_{};
    bool invalidated_ = false;
};
}  // namespace rhythm::studio
