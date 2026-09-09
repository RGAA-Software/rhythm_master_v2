#include "runtime_state.h"

namespace rhythm::runtime {
Runtime::Runtime() : impl_(std::make_unique<Impl>()) {}
Runtime::~Runtime() = default;
Runtime::Runtime(Runtime&&) noexcept = default;
Runtime& Runtime::operator=(Runtime&&) noexcept = default;
FrameResult Runtime::EvaluateSafely(const graph::ExecutionPlan& plan, FrameContext frame,
                                    render::Renderer& renderer) {
    return impl_->EvaluateSafely(plan, std::move(frame), renderer);
}
void Runtime::Reset() { impl_->Reset(); }
void Runtime::BeginPreparation(graph::ExecutionPlan plan, FrameContext frame) {
    impl_->BeginPreparation(std::move(plan), std::move(frame));
}
PreparationProgress Runtime::PrepareNext(render::Renderer& renderer, PreparationBudget budget) {
    return impl_->PrepareNext(renderer, budget);
}
FrameResult Runtime::Evaluate(const graph::ExecutionPlan& plan, FrameContext frame,
                              render::Renderer& renderer) {
    return impl_->Evaluate(plan, frame, renderer);
}
}  // namespace rhythm::runtime
