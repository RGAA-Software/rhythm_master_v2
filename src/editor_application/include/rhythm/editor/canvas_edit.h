#pragma once

#include "rhythm/editor/commands.h"
#include "rhythm/geometry2d/affine.h"

namespace rhythm::editor {
struct CanvasTarget {
    graph::NodeId node_ = 0;
    geometry2d::Size canvas_{};
    geometry2d::Pose pose_{};
    geometry2d::Affine parent_{};
    double uniform_scale_ = 1;
};
using CanvasInspection = std::variant<CanvasTarget, graph::Diagnostic>;
// Selected author affine -> unique coordinate-preserving route to final output.
// Ambiguous branches, driven transforms and singular/invisible targets reject.
CanvasInspection InspectCanvasTarget(const Snapshot& snapshot, graph::NodeId node);
enum class CanvasGesture { kTranslate, kRotate, kScale, kPivot };
struct CanvasSnap {
    double pixels_ = 0;
    double degrees_ = 0;
    double scale_ = 0;
};
// UI-thread value draft. No History mutation until Finish is explicitly applied;
// callers cancel by destroying the draft on Esc, focus/selection loss or revision change.
class CanvasEdit final {
   public:
    CanvasEdit(Snapshot base, graph::NodeId node, CanvasGesture gesture, geometry2d::Point start);
    bool Update(geometry2d::Point point, CanvasSnap snap = {});
    const Snapshot& Preview() const { return draft_; }
    EditResult Finish(const Snapshot& current) const;
    const CanvasTarget& Target() const { return target_; }

   private:
    Snapshot draft_{};
    CanvasTarget target_{};
    CanvasGesture gesture_ = CanvasGesture::kTranslate;
    geometry2d::Point start_{};
    geometry2d::Affine inverse_parent_{};
};
}  // namespace rhythm::editor
