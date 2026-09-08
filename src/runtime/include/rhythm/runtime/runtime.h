#pragma once

#include <map>

#include "rhythm/assets/images.h"
#include "rhythm/graph/compiler.h"
#include "rhythm/image_shader/resources.h"
#include "rhythm/particles/types.h"
#include "rhythm/render/budget.h"
#include "rhythm/render/renderer.h"
#include "rhythm/runtime/inputs.h"
#include "rhythm/runtime/video_inputs.h"
#include "rhythm/scene/camera.h"
#include "rhythm/scene/path.h"
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
    std::shared_ptr<const assets::Images> images_{};
    std::vector<VideoInput> videos_{};
    // nullopt preserves every node texture (inspection/compatibility mode).
    // A list enables dynamic intermediate reuse: only final, static/history and
    // listed node textures survive evaluation. Other outputs have empty handles.
    std::optional<std::vector<graph::NodeId>> retained_textures_{};
    bool profile_nodes_ = false;
    std::shared_ptr<const image_shader::Resources> shaders_{};
    bool operator==(const FrameContext&) const = default;
};
// Observers of a scene capture, with the exact projection used to write depth.
// Runtime owns both attachments. Depth is data, never an ordinary color texture.
struct DepthView {
    render::TextureHandle texture_{};
    render::DepthLinearization projection_{};
};
struct SceneImage {
    render::TextureHandle color_{};
    DepthView depth_{};
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
    // Borrowed value handle; Runtime owns the mutable GPU state.
    render::GpuPointHandle gpu_points_{};
    std::optional<SceneImage> scene_image_{};
    std::optional<DepthView> depth_{};
    std::shared_ptr<const scene::Path> path_{};
};
// Optional host-thread CPU/submission measurements, not GPU timestamp timings.
struct NodeProfile {
    graph::NodeId node_ = 0;
    double cpu_ms_ = 0;
    std::uint32_t passes_ = 0;
    std::int64_t texture_delta_bytes_ = 0;
};
struct FrameResult {
    std::vector<NodeOutput> outputs_{};
    render::TextureHandle final_{};
    std::uint32_t evaluated_ = 0;
    render::Extent extent_{640, 360};
    // No output handles are returned after a resource admission failure.
    std::optional<render::Budget> budget_{};
    std::uint32_t recycled_textures_ = 0;
    std::vector<NodeProfile> profiles_{};
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
    // Host-facing evaluation: release partial resources and latch budget failures.
    // Retry when the plan, extent, reset generation or prepared resources change,
    // or after Reset(). Time/audio changes alone never retry a rejected scene.
    FrameResult EvaluateSafely(const graph::ExecutionPlan& plan, FrameContext frame,
                               render::Renderer& renderer);
    void Reset();

   private:
    class Impl;
    std::unique_ptr<Impl> impl_{};
};
}  // namespace rhythm::runtime
