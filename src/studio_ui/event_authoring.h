#pragma once

#include <set>

#include "rhythm/editor/history.h"
#include "rhythm/runtime/runtime.h"

namespace rhythm::studio {
// UI/evaluation-thread owner of local requests and one recording take. Queue
// admission precedes evaluation; only observed manual output is recorded.
class EventAuthoring final {
   public:
    std::shared_ptr<const parameters::EventBatch> Advance(const graph::Document& document,
                                                          const graph::ExecutionPlan& plan,
                                                          const runtime::FrameContext& frame,
                                                          bool current_plan);
    void Observe(const runtime::FrameResult& frame);
    bool Trigger(graph::NodeId target, parameters::EventKind kind, double value);
    bool Record(const editor::Snapshot& snapshot, graph::NodeId target);
    std::optional<editor::Snapshot> Finish(const editor::Snapshot& snapshot);
    void Cancel();
    std::optional<editor::Snapshot> Draw(const editor::Snapshot& snapshot, graph::NodeId selected,
                                         const std::map<std::string, std::string>& text);
    bool Recording() const { return recorder_.Active(); }
    std::size_t Captured() const { return recorder_.Captured(); }
    const std::string& Status() const { return status_; }

   private:
    parameters::EventQueue queue_{};
    parameters::EventRecorder recorder_{};
    std::optional<parameters::EventTrack> base_{};
    std::set<graph::NodeId> available_{};
    graph::NodeId target_ = 0;
    std::string document_{};
    std::string status_{};
    std::uint64_t generation_ = 0;
    std::uint64_t revision_ = 0;
    std::uint64_t sequence_ = 1;
    double seconds_ = 0;
    bool ready_ = false;
};
}  // namespace rhythm::studio
