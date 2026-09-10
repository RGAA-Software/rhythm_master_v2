#include <cmath>
#include <stdexcept>

#include "rhythm/graph/video_clip.h"
#include "rhythm/player/session.h"
#include "rhythm/prepared_assets/prepare.h"

namespace rhythm::player {
runtime::PreparationProgress Session::PrepareGraphics(double monotonic_seconds,
                                                      render::Extent extent,
                                                      render::Renderer& renderer,
                                                      const runtime::ExternalInputs& inputs,
                                                      const runtime::PlaybackSample& playback,
                                                      runtime::PreparationBudget budget) {
    if (!package_ || !extent.width_ || !extent.height_ || !runtime::ValidExternalInputs(inputs) ||
        !std::isfinite(monotonic_seconds) || !std::isfinite(playback.seconds_) ||
        playback.seconds_ < 0 || !budget.maximum_nodes_ || budget.maximum_nodes_ > 4096 ||
        !std::isfinite(budget.maximum_cpu_ms_) || budget.maximum_cpu_ms_ <= 0 ||
        budget.maximum_cpu_ms_ > 100)
        throw std::invalid_argument("player.preparation_input");
    if (preparation_context_ && extent != extent_) ReleaseGraphics();
    if (!preparation_context_) {
        ReleaseGraphics();
        clock_.Advance(monotonic_seconds, false, playback);
        clock_generation_ = clock_.Generation();
        extent_ = extent;
        runtime::FrameContext context{Seconds(), generation_, extent, false};
        context.motion_ = clock_.Motion();
        context.resources_ = resources_->models_;
        context.images_ = resources_->images_;
        context.shaders_ = resources_->shaders_;
        context.surfaces_ = resources_->surfaces_;
        context.external_ = inputs;
        context.external_.controls_ = parameters::EvaluateControls(Controls(), ControlSequence(),
                                                                   Seconds(), inputs.controls_);
        context.advance_state_ = false;
        context.retained_textures_ = std::vector<graph::NodeId>{};
        preparation_context_ = std::move(context);
        preparation_progress_.total_nodes_ = package_->program_.instructions_.size();
        video_wait_started_ = std::chrono::steady_clock::now();
    }
    if (preparation_progress_.state_ != runtime::PreparationState::kPending)
        return preparation_progress_;
    try {
        if (!preparation_started_) {
            auto& context = *preparation_context_;
            context.videos_ = videos_.Update(package_->program_, *resources_, context.seconds_,
                                             context.reset_generation_);
            if (!videos_.Error().empty()) throw std::runtime_error(videos_.Error());
            std::size_t expected_videos = 0;
            for (const auto& instruction : package_->program_.instructions_) {
                if (instruction.operation_ != graph::Operation::kTextureVideo) continue;
                if (instruction.node_.type_ == "texture.video_clip" &&
                    !graph::DescribeVideoClip(instruction.node_).Sample(context.seconds_).active_)
                    continue;
                ++expected_videos;
            }
            if (context.videos_.size() != expected_videos) {
                if (std::chrono::steady_clock::now() - video_wait_started_ >
                    std::chrono::seconds(10))
                    throw std::runtime_error("player.preparation_video_timeout");
                return preparation_progress_;
            }
            runtime_.BeginPreparation(package_->program_, context);
            preparation_started_ = true;
        }
        preparation_progress_ = runtime_.PrepareNext(renderer, budget);
        if (preparation_progress_.state_ == runtime::PreparationState::kReady) {
            frame_ = *preparation_progress_.output_;
            external_ = preparation_context_->external_;
        }
    } catch (const std::exception& error) {
        runtime_.Reset();
        preparation_progress_.state_ = runtime::PreparationState::kFailed;
        preparation_progress_.error_ = error.what();
        preparation_progress_.output_.reset();
    }
    return preparation_progress_;
}
}  // namespace rhythm::player
