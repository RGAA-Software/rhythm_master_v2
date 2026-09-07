#pragma once

#include "rhythm/particles/simulation.h"
#include "rhythm/runtime/runtime.h"

namespace rhythm::runtime::detail {
struct PointState {
    particles::Simulation simulation_{};
    std::optional<double> last_seconds_{};
    bool burst_high_ = false;
    bool catch_up_limited_ = false;
};
std::shared_ptr<const particles::PointCloud> EmitPoints(PointState& state,
                                                        const graph::Instruction& instruction,
                                                        std::span<const NodeOutput> outputs,
                                                        FrameContext frame);
std::shared_ptr<const particles::PointCloud> GridPoints(const graph::Node& node);
std::shared_ptr<const particles::PointCloud> TransformPoints(const graph::Instruction& instruction,
                                                             std::span<const NodeOutput> outputs,
                                                             double aspect = 1);
render::Texture CreatePointSprite(render::Renderer& renderer);
void DrawPoints(std::span<const particles::Point> points, render::TextureHandle sprite,
                render::BlendMode blend, render::DrawList& list);
}  // namespace rhythm::runtime::detail
