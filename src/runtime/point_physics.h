#pragma once

#include "rhythm/physics/world.h"
#include "rhythm/runtime/runtime.h"

namespace rhythm::runtime::detail {
// Owns point identity -> rigid body lifetime. Input positions/velocities seed new
// bodies; live body motion belongs to physics. Source removal retires the body.
class PointPhysics final {
   public:
    std::shared_ptr<const particles::PointCloud> Evaluate(const graph::Instruction& instruction,
                                                          std::span<const NodeOutput> outputs,
                                                          FrameContext frame);
    std::uint64_t Generation() const { return generation_; }

   private:
    struct Body {
        physics::BodyHandle handle_{};
        float size_ = 0;
    };
    void Configure(const graph::Node& node, double aspect);
    std::unique_ptr<physics::World> world_{};
    std::map<std::uint64_t, Body> bodies_{};
    std::optional<graph::Node> configuration_{};
    std::optional<double> last_seconds_{};
    double aspect_ = 1;
    graph::NodeId source_node_ = 0;
    std::uint64_t source_generation_ = 0;
    std::uint64_t generation_ = 0;
};
}  // namespace rhythm::runtime::detail
