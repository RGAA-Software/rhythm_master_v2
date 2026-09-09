#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>

#include "rhythm/graph/compiler.h"
#include "rhythm/graph/components.h"

namespace rhythm::editor {
struct ScopedViewers {
    std::vector<graph::NodeId> instance_path_{};
    std::vector<graph::NodeId> nodes_{};
};
struct Compilation {
    std::uint64_t generation_ = 0;
    graph::CompileResult result_{};
    std::vector<graph::NodeId> viewers_{};
    std::map<graph::NodeId, graph::NodeId> scoped_nodes_{};
    // Same expansion and generation as result_; UI never expands on mouse input.
    std::map<graph::NodeId, graph::AuthorNode> authors_{};
};
// One active job, one latest pending request, one result. No UI/render mutation.
class CompilerWorker final {
   public:
    CompilerWorker();
    ~CompilerWorker();
    std::uint64_t Submit(graph::Document document, std::vector<graph::NodeId> viewers = {},
                         ScopedViewers scoped = {});
    std::optional<Compilation> Take();

   private:
    struct Request {
        graph::Document document_{};
        std::vector<graph::NodeId> viewers_{};
        std::uint64_t generation_ = 0;
        ScopedViewers scoped_{};
    };
    void Run(std::stop_token stop);
    std::mutex mutex_{};
    std::condition_variable_any changed_{};
    std::optional<Request> pending_{};
    std::optional<Compilation> completed_{};
    std::uint64_t generation_ = 0;
    std::jthread worker_{};
};
}  // namespace rhythm::editor
