#include "event_authoring.h"

#include <algorithm>
#include <limits>

namespace rhythm::studio {
namespace {
std::optional<parameters::EventTrack> Track(const graph::Document& document, graph::NodeId id) {
    for (const auto& node : document.nodes_)
        if (node.id_ == id && node.type_ == "event.input") {
            const auto found = node.properties_.find("actions");
            if (found != node.properties_.end() &&
                std::holds_alternative<parameters::EventTrack>(found->second))
                return std::get<parameters::EventTrack>(found->second);
        }
    return std::nullopt;
}
}  // namespace
void EventAuthoring::Cancel() {
    if (recorder_.Active() || queue_.Pending()) status_ = "event.take_cancelled";
    recorder_.Cancel();
    base_.reset();
    target_ = 0;
    if (generation_) queue_.Reset(generation_, seconds_);
}
std::shared_ptr<const parameters::EventBatch> EventAuthoring::Advance(
        const graph::Document& document, const graph::ExecutionPlan& plan,
        const runtime::FrameContext& frame, bool current_plan) {
    if (frame.reset_generation_ == std::numeric_limits<std::uint64_t>::max()) {
        Cancel();
        ready_ = false;
        status_ = "event.take_interrupted";
        return {};
    }
    const auto generation = frame.reset_generation_ + 1;
    if (document_ != document.id_ || generation_ != generation || frame.seconds_ < seconds_) {
        const bool interrupted = recorder_.Active() || queue_.Pending();
        Cancel();
        generation_ = generation;
        document_ = document.id_;
        queue_.Reset(generation_, frame.seconds_);
        if (interrupted) status_ = "event.take_interrupted";
    }
    seconds_ = frame.seconds_;
    if (recorder_.Active() &&
        (revision_ != document.revision_ || Track(document, target_) != base_)) {
        Cancel();
        status_ = "event.take_target_changed";
    }
    revision_ = document.revision_;
    available_.clear();
    std::set<graph::NodeId> root_inputs;
    for (const auto& node : document.nodes_)
        if (node.type_ == "event.input") root_inputs.insert(node.id_);
    for (const auto& instruction : plan.instructions_)
        if (instruction.operation_ == graph::Operation::kEventInput &&
            root_inputs.contains(instruction.node_.id_))
            available_.insert(instruction.node_.id_);
    ready_ = current_plan && frame.advance_state_;
    const auto dispatch = queue_.Drain(seconds_, !ready_);
    if (dispatch.due_remaining_) status_ = "event.pending_actions";
    if (dispatch.batch_.Events().empty()) return {};
    return std::make_shared<const parameters::EventBatch>(dispatch.batch_);
}
bool EventAuthoring::Trigger(graph::NodeId target, parameters::EventKind kind, double value) {
    if (!ready_ || !available_.contains(target) || (recorder_.Active() && target != target_) ||
        sequence_ == std::numeric_limits<std::uint64_t>::max())
        return false;
    const parameters::Event event{seconds_,  {0, target, parameters::EventOrigin::kManual},
                                  sequence_, generation_,
                                  kind,      value};
    if (queue_.Submit(event) != parameters::EventAdmission::kAccepted) {
        status_ = "event.input_rejected";
        return false;
    }
    ++sequence_;
    status_ = "event.pending_actions";
    return true;
}
bool EventAuthoring::Record(const editor::Snapshot& snapshot, graph::NodeId target) {
    if (recorder_.Active() || !ready_ || !available_.contains(target) || queue_.Pending() ||
        snapshot.document_.id_ != document_ || snapshot.document_.revision_ != revision_)
        return false;
    base_ = Track(snapshot.document_, target);
    if (!base_) return false;
    target_ = target;
    recorder_.Begin(*base_, {0, target, parameters::EventOrigin::kManual}, generation_, seconds_);
    status_ = "event.recording";
    return true;
}
void EventAuthoring::Observe(const runtime::FrameResult& frame) {
    if (frame.budget_) {
        Cancel();
        status_ = "event.take_interrupted";
        return;
    }
    if (frame.rejected_events_) status_ = "event.input_rejected";
    for (const auto& output : frame.outputs_) {
        if (output.node_ != target_ || !output.events_ || !recorder_.Active()) continue;
        for (const auto& event : output.events_->Events()) {
            if (event.source_.origin_ != parameters::EventOrigin::kManual) continue;
            const auto admission = recorder_.Capture(event);
            if (admission == parameters::RecordingAdmission::kFull)
                status_ = "event.recording_full";
            else if (admission != parameters::RecordingAdmission::kRecorded &&
                     admission != parameters::RecordingAdmission::kDuplicateOrOld)
                status_ = "event.input_rejected";
        }
    }
    if (!queue_.Pending() && status_ == "event.pending_actions")
        status_ = recorder_.Active() ? "event.recording" : "event.dispatched";
}
std::optional<editor::Snapshot> EventAuthoring::Finish(const editor::Snapshot& snapshot) {
    if (!recorder_.Active() || queue_.Pending()) return {};
    if (snapshot.document_.id_ != document_ || Track(snapshot.document_, target_) != base_) {
        Cancel();
        status_ = "event.take_target_changed";
        return {};
    }
    auto next = snapshot;
    const auto track = recorder_.Finish();
    for (auto& node : next.document_.nodes_)
        if (node.id_ == target_) node.properties_["actions"] = track;
    const bool changed = !base_ || track != *base_;
    Cancel();
    status_ = "event.take_saved";
    return changed ? std::optional<editor::Snapshot>(std::move(next)) : std::nullopt;
}
}  // namespace rhythm::studio
