#pragma once

#include "gizmo.h"
#include "output_edit.h"
#include "rhythm/editor/scene_edit.h"
#include "scene_selection.h"

namespace rhythm::studio {
class SceneCanvas final {
   public:
    OutputEdit Draw(const editor::Snapshot& snapshot, graph::NodeId selected, std::uint64_t texture,
                    geometry2d::Size extent, bool editable, bool current_output, bool& enabled,
                    const std::map<std::string, std::string>& text,
                    std::span<const runtime::NodeOutput> outputs = {},
                    const std::map<graph::NodeId, graph::AuthorNode>& authors = {});
    bool Active() const { return edit_.has_value(); }
    const editor::Snapshot& Preview() const { return edit_.value().Preview(); }
    bool Cancel();

   private:
    Gizmo gizmo_{};
    std::optional<editor::SceneEdit> edit_{};
    std::optional<geometry2d::Rect> captured_viewport_{};
    GizmoOperation mode_ = GizmoOperation::kTranslate;
    bool world_ = false;
    bool snap_ = false;
    std::string error_{};
    graph::NodeId selected_ = 0;
    std::uint64_t revision_ = 0;
    std::optional<SceneSelection> selection_{};
    bool edit_batch_ = false;
};
}  // namespace rhythm::studio
