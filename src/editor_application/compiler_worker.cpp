#include "rhythm/editor/compiler_worker.h"

namespace rhythm::editor {
CompilerWorker::CompilerWorker() : worker_([this](std::stop_token stop) { Run(stop); }) {}
CompilerWorker::~CompilerWorker() {
    worker_.request_stop();
    changed_.notify_all();
}
std::uint64_t CompilerWorker::Submit(graph::Document document, std::vector<graph::NodeId> viewers) {
    const std::lock_guard lock(mutex_);
    pending_ = Request{std::move(document), std::move(viewers), ++generation_};
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
        graph::CompileResult result;
        try {
            result = graph::Compile(request.document_, registry, request.viewers_);
        } catch (const std::exception&) {
            result = std::vector<graph::Diagnostic>{{"graph.compiler_failure"}};
        }
        const std::lock_guard lock(mutex_);
        if (!stop.stop_requested() && request.generation_ == generation_)
            completed_ = Compilation{request.generation_, std::move(result)};
    }
}
}  // namespace rhythm::editor
