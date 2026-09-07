#include "rhythm/editor/history.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace rhythm::editor {
namespace {
graph::NodeId ObserveIds(const graph::Document& document, graph::NodeId next) {
    for (const auto& node : document.nodes_)
        next = std::max(next, node.id_ == std::numeric_limits<graph::NodeId>::max() ? node.id_
                                                                                    : node.id_ + 1);
    return next;
}
}  // namespace
History::History(Snapshot initial)
    : current_(std::move(initial)),
      revision_(current_.document_.revision_),
      next_node_id_(ObserveIds(current_.document_, 1)) {}
void History::Install(Snapshot snapshot) {
    if (revision_ == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error("editor.revision_limit");
    snapshot.document_.revision_ = ++revision_;
    next_node_id_ = ObserveIds(snapshot.document_, next_node_id_);
    current_ = std::move(snapshot);
}
bool History::Apply(Snapshot next, std::uint64_t expected_revision) {
    if (expected_revision != revision_ || next.document_.id_ != current_.document_.id_)
        return false;
    next.document_.revision_ = revision_;
    if (next == current_) return false;
    if (undo_.size() == 100) undo_.erase(undo_.begin());
    undo_.push_back(current_);
    redo_.clear();
    Install(std::move(next));
    return true;
}
bool History::Undo() {
    if (undo_.empty()) return false;
    redo_.push_back(current_);
    auto next = std::move(undo_.back());
    undo_.pop_back();
    Install(std::move(next));
    return true;
}
bool History::Redo() {
    if (redo_.empty()) return false;
    undo_.push_back(current_);
    auto next = std::move(redo_.back());
    redo_.pop_back();
    Install(std::move(next));
    return true;
}
graph::NodeId History::ReserveNodeId() {
    for (const auto& node : current_.document_.nodes_) {
        if (node.id_ == std::numeric_limits<graph::NodeId>::max())
            throw std::overflow_error("graph.id_exhausted");
        next_node_id_ = std::max(next_node_id_, node.id_ + 1);
    }
    if (next_node_id_ == std::numeric_limits<graph::NodeId>::max())
        throw std::overflow_error("graph.id_exhausted");
    return next_node_id_++;
}
}  // namespace rhythm::editor
