#pragma once

#include "rhythm/editor/canvas_edit.h"

namespace rhythm::studio {
struct OutputEdit {
    std::optional<editor::Snapshot> committed_{};
    bool preview_changed_ = false;
};
// UI-thread image viewport and captured author-transform gesture. History and
// compilation remain application responsibilities; no native resources are owned.
class OutputCanvas final {
   public:
    OutputEdit Draw(const editor::Snapshot& snapshot, graph::NodeId selected, std::uint64_t texture,
                    geometry2d::Size extent, bool editable, bool current_output,
                    const std::map<std::string, std::string>& text);
    bool Active() const { return edit_.has_value(); }
    const editor::Snapshot& Preview() const { return edit_.value().Preview(); }
    bool Cancel();

   private:
    std::optional<editor::CanvasEdit> edit_{};
    bool enabled_ = false;
    bool snap_ = false;
    editor::CanvasGesture mode_ = editor::CanvasGesture::kTranslate;
};
}  // namespace rhythm::studio
