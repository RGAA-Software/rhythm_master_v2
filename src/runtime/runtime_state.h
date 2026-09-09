#pragma once

#include <functional>

#include "blur_pass.h"
#include "event_ops.h"
#include "gpu_particle_pass.h"
#include "image_pass.h"
#include "point_ops.h"
#include "point_physics.h"
#include "scene_capture.h"
#include "scene_pass.h"
#include "shader_pass.h"
#include "texture_lifetimes.h"
#include "trail_pass.h"
#include "vector_pass.h"
#include "video_pass.h"

namespace rhythm::runtime {
class Runtime::Impl final {
   public:
    FrameResult Evaluate(const graph::ExecutionPlan& plan, FrameContext frame,
                         render::Renderer& renderer);
    FrameResult EvaluateSafely(const graph::ExecutionPlan& plan, FrameContext frame,
                               render::Renderer& renderer);
    void BeginPreparation(graph::ExecutionPlan plan, FrameContext frame);
    PreparationProgress PrepareNext(render::Renderer& renderer, PreparationBudget budget);
    void Reset();

   private:
    struct Preparation {
        graph::ExecutionPlan plan_{};
        FrameContext frame_{};
        FrameResult partial_{};
        std::size_t next_ = 0;
        bool initialized_ = false;
        bool redraw_ = false;
        std::uint64_t presentation_generation_ = 0;
        double maximum_node_ms_ = 0;
        std::uint32_t required_passes_ = 0;
    };
    FrameResult EvaluateRange(const graph::ExecutionPlan& plan, FrameContext frame,
                              render::Renderer& renderer,
                              const std::optional<std::reference_wrapper<Preparation>>& preparation,
                              PreparationBudget budget);
    void ResetResources();
    std::optional<Preparation> preparation_{};
    struct Failure {
        graph::ExecutionPlan plan_{};
        render::Extent extent_{};
        std::uint64_t reset_generation_ = 0;
        std::shared_ptr<const scene::Resources> resources_{};
        std::shared_ptr<const assets::Images> images_{};
        std::shared_ptr<const image_shader::Resources> shaders_{};
        std::optional<std::vector<graph::NodeId>> retained_{};
        render::Budget budget_ = render::Budget::kTextureBytes;
    };
    std::optional<Failure> failure_{};
    detail::TextureLifetimes lifetimes_{};
    detail::TexturePool targets_{};
    struct PausedFrame {
        FrameContext frame_{};
        FrameResult result_{};
        std::uint64_t plan_generation_ = 0;
        std::uint64_t presentation_generation_ = 0;
    };
    std::optional<PausedFrame> paused_frame_{};
    struct State {
        std::optional<graph::Node> node_{};
        std::vector<std::uint64_t> input_versions_{};
        NodeOutput output_{};
        render::Texture target_{};
        render::Texture history_{};
        render::Extent extent_{};
        render::TexturePrecision precision_ = render::TexturePrecision::kUnorm8;
        bool target_retired_ = false;
        std::unique_ptr<detail::PointState> points_{};
        std::unique_ptr<detail::GpuParticlePass> gpu_particles_{};
        std::unique_ptr<detail::PointPhysics> physics_{};
        std::unique_ptr<detail::ScenePass> scene_{};
        std::unique_ptr<detail::SceneCapture> capture_{};
        std::unique_ptr<detail::BlurPass> blur_{};
        std::unique_ptr<detail::TrailPass> trail_{};
        std::unique_ptr<detail::EventNode> events_{};
        std::uint64_t last_reset_sequence_ = 0;
    };
    std::map<graph::NodeId, State> states_{};
    render::Texture white_{};
    render::Texture point_sprite_{};
    detail::ImageUploads images_{};
    detail::ShaderPrograms shaders_{};
    detail::VideoUploads videos_{};
    detail::VectorMeshes vectors_{};
    std::string document_id_{};
    std::uint64_t reset_generation_ = 0;
    render::Extent extent_{};
    std::uint64_t next_points_generation_ = 1;
    std::uint64_t next_output_version_ = 1;
    // Keep monotonic IDs across per-node/resource reconstruction and graph edits.
    std::uint64_t next_event_sequence_ = 1;
    std::uint64_t presentation_generation_ = 0;
};
}  // namespace rhythm::runtime
