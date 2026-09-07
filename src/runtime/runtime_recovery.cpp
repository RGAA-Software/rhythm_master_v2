#include <algorithm>
#include <cmath>

#include "runtime_state.h"

namespace rhythm::runtime {
namespace {
FrameResult Rejected(render::Budget budget, render::Extent extent) {
    FrameResult result;
    result.budget_ = budget;
    result.extent_ = extent;
    return result;
}
}  // namespace

FrameResult Runtime::Impl::EvaluateSafely(const graph::ExecutionPlan& plan, FrameContext frame,
                                          render::Renderer& renderer) {
    if (!std::isfinite(frame.seconds_) || frame.seconds_ < 0)
        throw std::invalid_argument("runtime.frame");
    if (!ValidExternalInputs(frame.external_))
        throw std::invalid_argument("runtime.external_inputs");
    if (failure_) {
        if (failure_->extent_ == frame.extent_ &&
            failure_->reset_generation_ == frame.reset_generation_ &&
            failure_->resources_ == frame.resources_ && failure_->images_ == frame.images_ &&
            failure_->retained_ == frame.retained_textures_ &&
            detail::SamePlan(failure_->plan_, plan))
            return Rejected(failure_->budget_, frame.extent_);
        failure_.reset();
    }
    try {
        return Evaluate(plan, frame, renderer);
    } catch (const render::BudgetExceeded& error) {
        // Submitted commands retire at EndFrame. Backend RAII destruction is
        // deferred; no partially evaluated or stale handles escape to host UI.
        Reset();
        failure_ = Failure{plan,
                           frame.extent_,
                           frame.reset_generation_,
                           frame.resources_,
                           frame.images_,
                           frame.retained_textures_,
                           error.Kind()};
        return Rejected(error.Kind(), frame.extent_);
    }
}
}  // namespace rhythm::runtime
