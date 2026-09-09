#include "rhythm/player/performance_actions.h"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace rhythm::player {
namespace {
std::size_t Index(PerformanceActionKind kind) {
    switch (kind) {
        case PerformanceActionKind::kSnapshot:
            return 0;
        case PerformanceActionKind::kNextScene:
            return 1;
    }
    throw std::invalid_argument("performance.action_kind");
}
bool Outstanding(const PerformanceAction& action) {
    return action.state_ == PerformanceActionState::kPending ||
           action.state_ == PerformanceActionState::kDispatched;
}
}  // namespace
void PerformanceActions::Observe(const runtime::PlaybackSample& sample,
                                 std::uint64_t document_generation,
                                 const std::optional<parameters::BeatSettings>& grid,
                                 bool suspended) {
    if (!std::isfinite(sample.seconds_) || sample.seconds_ < 0 ||
        sample.seconds_ > parameters::kMaximumBeatSeconds ||
        (sample.duration_ && (!std::isfinite(*sample.duration_) || *sample.duration_ <= 0)) ||
        (grid && !parameters::ValidBeatSettings(*grid)))
        throw std::invalid_argument("performance.sample");
    if (sample_ &&
        (sample_->generation_ != sample.generation_ ||
         document_generation_ != document_generation || sample.seconds_ < sample_->seconds_))
        CancelAll(PerformanceActionReason::kSourceChanged);
    else if (sample_ && grid_ != grid)
        CancelAll(PerformanceActionReason::kGridChanged);
    sample_ = sample;
    grid_ = grid;
    document_generation_ = document_generation;
    suspended_ = suspended;
}
std::uint64_t PerformanceActions::Request(PerformanceActionKind kind, std::uint64_t target,
                                          parameters::Quantization quantization) {
    auto& previous = actions_.at(Index(kind));
    if (!sample_ || !target ||
        (quantization != parameters::Quantization::kImmediate &&
         quantization != parameters::Quantization::kBeat &&
         quantization != parameters::Quantization::kBar))
        throw std::invalid_argument("performance.request");
    if (Outstanding(previous) && previous.target_ == target &&
        previous.quantization_ == quantization)
        return previous.id_;
    if (next_id_ == std::numeric_limits<std::uint64_t>::max())
        throw std::length_error("performance.id_exhausted");
    PerformanceAction next{next_id_++, kind, target, quantization};
    if (Outstanding(previous)) next.replaced_id_ = previous.id_;
    next.state_ = PerformanceActionState::kPending;
    if (quantization == parameters::Quantization::kImmediate) {
        next.due_seconds_ = sample_->seconds_;
    } else if (!grid_) {
        next.state_ = PerformanceActionState::kFailed;
        next.reason_ = PerformanceActionReason::kNoGrid;
    } else {
        next.due_seconds_ = parameters::BeatGrid(*grid_).NextAfter(sample_->seconds_, quantization);
        if (!next.due_seconds_) {
            next.state_ = PerformanceActionState::kFailed;
            next.reason_ = PerformanceActionReason::kNoBoundary;
        }
    }
    previous = next;
    return next.id_;
}
std::optional<PerformanceAction> PerformanceActions::TakeDue(PerformanceActionKind kind) {
    auto& action = actions_.at(Index(kind));
    if (!sample_ || suspended_ || sample_->paused_ ||
        action.state_ != PerformanceActionState::kPending || !action.due_seconds_ ||
        sample_->seconds_ < *action.due_seconds_)
        return {};
    action.state_ = PerformanceActionState::kDispatched;
    return action;
}
bool PerformanceActions::Resolve(std::uint64_t id, bool succeeded) {
    for (auto& action : actions_)
        if (action.id_ == id && action.state_ == PerformanceActionState::kDispatched) {
            action.state_ = succeeded ? PerformanceActionState::kCompleted
                                      : PerformanceActionState::kFailed;
            action.reason_ = succeeded ? PerformanceActionReason::kNone
                                       : PerformanceActionReason::kTargetUnavailable;
            return true;
        }
    return false;
}
bool PerformanceActions::Cancel(std::uint64_t id) {
    for (auto& action : actions_)
        if (action.id_ == id && Outstanding(action)) {
            action.state_ = PerformanceActionState::kCancelled;
            action.reason_ = PerformanceActionReason::kUser;
            return true;
        }
    return false;
}
void PerformanceActions::CancelAll(PerformanceActionReason reason) {
    for (auto& action : actions_)
        if (Outstanding(action)) {
            action.state_ = PerformanceActionState::kCancelled;
            action.reason_ = reason;
        }
}
const PerformanceAction& PerformanceActions::Status(PerformanceActionKind kind) const {
    return actions_.at(Index(kind));
}
}  // namespace rhythm::player
