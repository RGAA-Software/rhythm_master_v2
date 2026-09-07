#include "preview_routing.h"

#include <algorithm>
#include <utility>

#include "rhythm/runtime/viewers.h"

namespace rhythm::studio {
PreviewRequest PreviewRouting::Prepare(std::vector<graph::NodeId> roots,
                                       editor::ScopedViewers scoped) {
    if (scoped.nodes_.size() > runtime::Viewers::kMaxPreviews)
        scoped.nodes_.resize(runtime::Viewers::kMaxPreviews);
    const auto remaining = runtime::Viewers::kMaxPreviews - scoped.nodes_.size();
    if (roots.size() > remaining) roots.resize(remaining);
    scoped_nodes_.clear();
    invalidated_ = true;
    return {std::move(roots), std::move(scoped)};
}
void PreviewRouting::Stage(const editor::Compilation& compilation) {
    pending_nodes_ = compilation.viewers_;
    pending_scoped_nodes_ = compilation.scoped_nodes_;
    pending_signal_nodes_.clear();
    if (const auto plan = std::get_if<graph::ExecutionPlan>(&compilation.result_)) {
        const graph::Registry registry;
        for (const auto& instruction : plan->instructions_) {
            if (std::find(pending_nodes_.begin(), pending_nodes_.end(), instruction.node_.id_) ==
                pending_nodes_.end())
                continue;
            const auto descriptor = registry.Find(instruction.node_.type_);
            if (descriptor && (descriptor->output_ == graph::ValueType::kScalar ||
                               descriptor->output_ == graph::ValueType::kSignal))
                pending_signal_nodes_.push_back(instruction.node_.id_);
        }
    }
}
void PreviewRouting::Commit() {
    active_nodes_ = std::move(pending_nodes_);
    signal_nodes_ = std::move(pending_signal_nodes_);
    scoped_nodes_ = std::move(pending_scoped_nodes_);
    invalidated_ = true;
}
bool PreviewRouting::TakeInvalidation() { return std::exchange(invalidated_, false); }
CanvasPreviews PreviewRouting::Scoped(const CanvasPreviews& previews) const {
    CanvasPreviews result;
    result.enabled_ = previews.enabled_;
    for (const auto& [local, expanded] : scoped_nodes_) {
        if (const auto texture = previews.textures_.find(expanded);
            texture != previews.textures_.end())
            result.textures_[local] = texture->second;
        if (const auto signal = previews.signals_.find(expanded); signal != previews.signals_.end())
            result.signals_[local] = signal->second;
    }
    return result;
}
}  // namespace rhythm::studio
