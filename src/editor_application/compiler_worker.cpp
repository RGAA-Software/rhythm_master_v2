#include "rhythm/editor/compiler_worker.h"

#include <stdexcept>

#include "rhythm/graph/components.h"

namespace rhythm::editor {
CompilerWorker::CompilerWorker() : worker_([this](std::stop_token stop) { Run(stop); }) {}
CompilerWorker::~CompilerWorker() {
    worker_.request_stop();
    changed_.notify_all();
}
std::uint64_t CompilerWorker::Submit(graph::Document document, std::vector<graph::NodeId> viewers,
                                     ScopedViewers scoped) {
    const std::lock_guard lock(mutex_);
    pending_ = Request{std::move(document), std::move(viewers), ++generation_, std::move(scoped)};
    completed_.reset();
    changed_.notify_one();
    return generation_;
}
std::optional<Compilation> CompilerWorker::Take() {
    const std::lock_guard lock(mutex_);
    auto result = std::move(completed_);
    completed_.reset();
    return result;
}
void CompilerWorker::Run(std::stop_token stop) {
    const graph::Registry registry;
    while (!stop.stop_requested()) {
        Request request;
        {
            std::unique_lock lock(mutex_);
            if (!changed_.wait(lock, stop, [&] { return pending_.has_value(); })) return;
            request = std::move(*pending_);
            pending_.reset();
        }
        Compilation result;
        result.generation_ = request.generation_;
        result.viewers_ = request.viewers_;
        try {
            if (request.viewers_.size() + request.scoped_.nodes_.size() > 16)
                throw std::length_error("graph.limit");
            if (request.scoped_.nodes_.empty() && request.document_.components_.empty()) {
                result.result_ = graph::Compile(request.document_, registry, request.viewers_);
                for (const auto& node : request.document_.nodes_)
                    result.authors_.emplace(node.id_, graph::AuthorNode{{}, node.id_});
            } else {
                auto expanded = graph::ExpandComponentScope(
                        request.document_, registry,
                        request.scoped_.nodes_.empty() ? std::span<const graph::NodeId>{}
                                                       : request.scoped_.instance_path_);
                if (std::holds_alternative<std::vector<graph::Diagnostic>>(expanded)) {
                    result.result_ = std::get<std::vector<graph::Diagnostic>>(std::move(expanded));
                } else {
                    auto& scope = std::get<graph::ExpandedComponentScope>(expanded);
                    for (const auto id : request.scoped_.nodes_) {
                        const auto found = scope.nodes_.find(id);
                        if (found == scope.nodes_.end())
                            throw std::invalid_argument("graph.missing_viewer");
                        result.scoped_nodes_[id] = found->second;
                        result.viewers_.push_back(found->second);
                    }
                    result.result_ = graph::Compile(scope.document_, registry, result.viewers_);
                    result.authors_ = std::move(scope.authors_);
                }
            }
        } catch (const std::exception&) {
            result.result_ = std::vector<graph::Diagnostic>{{"graph.compiler_failure"}};
        }
        const std::lock_guard lock(mutex_);
        if (!stop.stop_requested() && request.generation_ == generation_)
            completed_ = std::move(result);
    }
}
}  // namespace rhythm::editor
