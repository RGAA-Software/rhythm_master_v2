#include <chrono>
#include <cmath>
#include <stdexcept>

#include "runtime_state.h"

namespace rhythm::runtime {
void Runtime::Impl::Reset() {
    preparation_.reset();
    ResetResources();
}
FrameResult Runtime::Impl::Evaluate(const graph::ExecutionPlan& plan, FrameContext frame,
                                    render::Renderer& renderer) {
    if (preparation_) throw std::logic_error("runtime.preparation_pending");
    return EvaluateRange(plan, std::move(frame), renderer, {}, {});
}
void Runtime::Impl::BeginPreparation(graph::ExecutionPlan plan, FrameContext frame) {
    if (!std::isfinite(frame.seconds_) || frame.seconds_ < 0 || !frame.extent_.width_ ||
        !frame.extent_.height_ || plan.instructions_.empty() ||
        plan.output_ >= plan.instructions_.size() || !ValidExternalInputs(frame.external_))
        throw std::invalid_argument("runtime.preparation_frame");
    frame.advance_state_ = false;
    Preparation incoming{std::move(plan), std::move(frame)};
    Reset();
    preparation_ = std::move(incoming);
}
PreparationProgress Runtime::Impl::PrepareNext(render::Renderer& renderer,
                                               PreparationBudget budget) {
    if (!preparation_) throw std::logic_error("runtime.preparation_missing");
    if (!budget.maximum_nodes_ || budget.maximum_nodes_ > 4096 ||
        !std::isfinite(budget.maximum_cpu_ms_) || budget.maximum_cpu_ms_ <= 0 ||
        budget.maximum_cpu_ms_ > 100)
        throw std::invalid_argument("runtime.preparation_budget");
    const auto started = std::chrono::steady_clock::now();
    PreparationProgress result;
    result.total_nodes_ = preparation_->plan_.instructions_.size();
    const auto previous_passes = renderer.Stats().passes_;
    try {
        auto output = EvaluateRange(preparation_->plan_, preparation_->frame_, renderer,
                                    std::ref(*preparation_), budget);
        result.completed_nodes_ = preparation_->next_;
        result.maximum_node_ms_ = preparation_->maximum_node_ms_;
        preparation_->required_passes_ += renderer.Stats().passes_ - previous_passes;
        result.required_passes_ = preparation_->required_passes_;
        if (result.required_passes_ > render::kMaximumOffscreenPasses)
            throw render::BudgetExceeded(render::Budget::kPasses);
        if (result.completed_nodes_ == result.total_nodes_) {
            if (!renderer.IsValid(output.final_))
                throw std::runtime_error("runtime.preparation_output");
            result.state_ = PreparationState::kReady;
            result.output_ = std::move(output);
            preparation_.reset();
        }
    } catch (const render::BudgetExceeded& error) {
        result.state_ = PreparationState::kFailed;
        result.budget_ = error.Kind();
        result.error_ = error.what();
        Reset();
    } catch (const std::exception& error) {
        result.state_ = PreparationState::kFailed;
        result.error_ = error.what();
        Reset();
    }
    result.cpu_ms_ =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started)
                    .count();
    return result;
}
}  // namespace rhythm::runtime
