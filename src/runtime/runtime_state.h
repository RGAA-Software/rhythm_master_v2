#pragma once

#include "blur_pass.h"
#include "image_pass.h"
#include "point_ops.h"
#include "point_physics.h"
#include "scene_pass.h"
#include "trail_pass.h"
#include "video_pass.h"

namespace rhythm::runtime {
class Runtime::Impl final {
   public:
    FrameResult Evaluate(const graph::ExecutionPlan& plan, FrameContext frame,
                         render::Renderer& renderer);
    FrameResult EvaluateSafely(const graph::ExecutionPlan& plan, FrameContext frame,
                               render::Renderer& renderer);
    void Reset();

   private:
    struct Failure {
        graph::ExecutionPlan plan_{};
        render::Extent extent_{};
        std::uint64_t reset_generation_ = 0;
        std::shared_ptr<const scene::Resources> resources_{};
        std::shared_ptr<const assets::Images> images_{};
        render::Budget budget_ = render::Budget::kTextureBytes;
    };
    std::optional<Failure> failure_{};
    struct State {
        std::optional<graph::Node> node_{};
        std::vector<std::uint64_t> input_versions_{};
        NodeOutput output_{};
        render::Texture target_{};
        render::Texture history_{};
        render::Extent extent_{};
        std::unique_ptr<detail::PointState> points_{};
        std::unique_ptr<detail::PointPhysics> physics_{};
        std::unique_ptr<detail::ScenePass> scene_{};
        std::unique_ptr<detail::BlurPass> blur_{};
        std::unique_ptr<detail::TrailPass> trail_{};
    };
    std::map<graph::NodeId, State> states_{};
    render::Texture white_{};
    render::Texture point_sprite_{};
    detail::ImageUploads images_{};
    detail::VideoUploads videos_{};
    std::string document_id_{};
    std::uint64_t reset_generation_ = 0;
    render::Extent extent_{};
    std::uint64_t next_points_generation_ = 1;
    std::uint64_t next_output_version_ = 1;
    std::uint64_t presentation_generation_ = 0;
};
}  // namespace rhythm::runtime
