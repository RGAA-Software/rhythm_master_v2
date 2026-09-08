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
    PreviewRequest Prepare(std::vector<graph::NodeId> roots, editor::ScopedViewers scoped,
                           std::string document_id = {});
    void DrawNavigation(const std::map<std::string, std::string>& text);
    bool StepPage(int direction);
    bool TakePageChange();
    std::size_t Page() const { return page_; }
    std::size_t Pages() const { return pages_; }
    void Stage(const editor::Compilation& compilation);
    void Commit();
    bool TakeInvalidation();
    std::span<const graph::NodeId> ActiveNodes() const { return active_nodes_; }
    std::span<const graph::NodeId> SignalNodes() const { return signal_nodes_; }
    CanvasPreviews Scoped(const CanvasPreviews& previews) const;

   private:
    std::vector<graph::NodeId> active_nodes_{};
    std::vector<graph::NodeId> pending_nodes_{};
    std::vector<graph::NodeId> signal_nodes_{};
    std::vector<graph::NodeId> pending_signal_nodes_{};
    std::map<graph::NodeId, graph::NodeId> scoped_nodes_{};
    std::map<graph::NodeId, graph::NodeId> pending_scoped_nodes_{};
    bool invalidated_ = false;
    PreviewRequest demand_{};
    std::string document_id_{};
    std::size_t page_ = 0;
    std::size_t pages_ = 0;
    bool page_changed_ = false;
};
}  // namespace rhythm::studio
