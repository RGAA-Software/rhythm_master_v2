#pragma once

#include "component_interface.h"
#include "graph_canvas.h"
#include "node_palette.h"
#include "property_inspector.h"
#include "rhythm/editor/compiler_worker.h"
#include "rhythm/editor/component_edit.h"
#include "time_track_editor.h"

namespace rhythm::studio {
class PreviewRouting;
// Owns nested authoring navigation and its draft lifecycle; application history
// receives one validated value only when the user applies the draft.
class ComponentWorkbench final {
   public:
    void Open(const editor::Snapshot& project, std::string type, graph::NodeId instance = 0);
    bool OpenAuthor(const editor::Snapshot& project, const graph::AuthorNode& author);
    std::optional<editor::Snapshot> Draw(const editor::Snapshot& project,
                                         const graph::Registry& registry,
                                         const std::map<std::string, std::string>& text,
                                         const std::string& locale, PreviewRouting& routing,
                                         const CanvasPreviews& previews = {});
    const std::optional<graph::Document>& PreviewDocument() const { return preview_document_; }
    editor::ScopedViewers PreviewViewers() const { return {instance_path_, preview_nodes_}; }
    std::uint64_t PreviewGeneration() const { return preview_generation_; }
    std::size_t DrawnPreviews() const { return edit_ ? canvas_.DrawnPreviews() : 0; }

   private:
    void CommitPreview();
    void ResetView();
    void DrawTiming(const std::map<std::string, std::string>& text);
    void UpdatePreview(const editor::Snapshot& project, const graph::Registry& registry,
                       bool visible);
    std::optional<editor::ComponentEdit> edit_{};
    GraphCanvas canvas_{};
    NodePalette node_palette_{};
    PropertyInspector inspector_{};
    ComponentInterface interface_{};
    TimeTrackEditor timing_{};
    double timing_duration_ = 16;
    double timing_insert_ = 0;
    std::string status_{};
    std::vector<graph::NodeId> instance_path_{};
    std::vector<graph::NodeId> preview_nodes_{};
    std::vector<graph::NodeId> preview_path_{};
    std::optional<graph::Document> preview_body_{};
    std::optional<graph::Document> preview_document_{};
    std::uint64_t source_revision_ = 0;
    std::string source_id_{};
    std::uint64_t preview_generation_ = 0;
};
}  // namespace rhythm::studio
