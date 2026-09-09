#pragma once

#include <memory>
#include <span>

#include "rhythm/editor/history.h"
#include "rhythm/runtime/signal_previews.h"

namespace rhythm::studio {
struct CanvasPreviews {
    bool enabled_ = false;
    // Frame-local UI texture IDs; native/backend handles stay inside adapters.
    std::map<graph::NodeId, std::uint64_t> textures_{};
    std::map<graph::NodeId, runtime::SignalTrace> signals_{};
    bool current_ = true;
};
class GraphCanvas final {
   public:
    GraphCanvas();
    ~GraphCanvas();
    std::optional<editor::Snapshot> Draw(const editor::Snapshot& snapshot,
                                         const graph::Registry& registry,
                                         const std::map<std::string, std::string>& labels,
                                         const CanvasPreviews& previews = {});
    graph::NodeId Selection() const;
    std::span<const graph::NodeId> Selections() const;
    void Select(graph::NodeId node);
    // UI-thread requests, applied after the next usable canvas layout.
    void FocusSelection();
    void FitContent();
    std::size_t VisibleNodes() const;
    editor::Position InsertionPoint() const;
    // UI-thread coordinate conversion for canvas interactions and input tests.
    editor::Position ToScreen(editor::Position position) const;
    std::span<const graph::NodeId> PreviewNodes() const;
    std::size_t DrawnPreviews() const;
    void RestoreLayout();

   private:
    class Impl;
    std::unique_ptr<Impl> impl_{};
};
}  // namespace rhythm::studio
