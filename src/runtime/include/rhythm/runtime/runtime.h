#pragma once

#include <map>

#include "rhythm/graph/compiler.h"
#include "rhythm/particles/types.h"
#include "rhythm/render/renderer.h"
#include "rhythm/runtime/inputs.h"
#include "rhythm/scene/camera.h"
#include "rhythm/scene/resources.h"
#include "rhythm/scene/scene.h"

namespace rhythm::runtime {
struct FrameContext {
    double seconds_ = 0;
    std::uint64_t reset_generation_ = 0;
    render::Extent extent_{640, 360};
    bool evaluate_viewers_ = true;
    render::Extent viewer_extent_{256, 144};
    ExternalInputs external_{};
    bool advance_state_ = true;
    std::shared_ptr<const scene::Resources> resources_{};
};
struct NodeOutput {
    graph::NodeId node_ = 0;
    double scalar_ = 0;
    render::TextureHandle texture_{};
    std::uint64_t version_ = 0;
    std::shared_ptr<const particles::PointCloud> points_{};
    // Identity generation changes when a source restarts and may reuse point IDs.
    std::uint64_t points_generation_ = 0;
    std::shared_ptr<const scene::Geometry> geometry_{};
    std::shared_ptr<const scene::Scene> scene_{};
    std::optional<scene::Material> material_{};
    std::optional<scene::Camera> camera_{};
};
struct FrameResult {
    std::vector<NodeOutput> outputs_{};
    render::TextureHandle final_{};
    std::uint32_t evaluated_ = 0;
    render::Extent extent_{640, 360};
};
// Evaluation and resource ownership are host-thread confined. The immutable plan
// may be compiled elsewhere; no UI or platform objects are retained here.
class Runtime final {
   public:
    Runtime();
    ~Runtime();
    Runtime(Runtime&&) noexcept;
    Runtime& operator=(Runtime&&) noexcept;
    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;
    FrameResult Evaluate(const graph::ExecutionPlan& plan, FrameContext frame,
                         render::Renderer& renderer);
    void Reset();

   private:
    class Impl;
    std::unique_ptr<Impl> impl_{};
};
}  // namespace rhythm::runtime
