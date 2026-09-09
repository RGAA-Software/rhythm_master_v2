#pragma once

#include "rhythm/editor/commands.h"
#include "rhythm/scene/camera.h"
#include "rhythm/scene/pose.h"

namespace rhythm::editor {
struct SceneTarget {
    graph::NodeId node_ = 0;
    graph::NodeId render_node_ = 0;
    graph::NodeId scene_source_ = 0;
    scene::EulerPose pose_{};
    scene::Matrix parent_{};
    scene::Camera camera_{};
    double uniform_scale_ = 1;
};
using SceneInspection = std::variant<SceneTarget, graph::Diagnostic>;
// Root author transform through one scene route and one unwarped final image.
// Driven parameters, ambiguous copies, nonlinear image warps and component
// expansion are explicit unsupported scopes, never guessed author identities.
SceneInspection InspectSceneTarget(const Snapshot& snapshot, graph::NodeId node);
class SceneEdit final {
   public:
    SceneEdit(Snapshot base, graph::NodeId node);
    bool Update(const scene::Matrix& local);
    EditResult Finish(const Snapshot& current) const;
    const Snapshot& Preview() const { return draft_; }
    const SceneTarget& Target() const { return target_; }
    const std::string& Error() const { return error_; }

   private:
    Snapshot draft_{};
    SceneTarget target_{};
    std::string error_{};
};
}  // namespace rhythm::editor
