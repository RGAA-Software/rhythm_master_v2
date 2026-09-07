#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>

#include "rhythm/graph/compiler.h"

namespace rhythm::editor {
struct Compilation {
    std::uint64_t generation_ = 0;
    graph::CompileResult result_{};
};
// One active job, one latest pending request, one result. No UI/render mutation.
class CompilerWorker final {
   public:
    CompilerWorker();
    ~CompilerWorker();
    std::uint64_t Submit(graph::Document document, std::vector<graph::NodeId> viewers = {});
    std::optional<Compilation> Take();

   private:
    struct Request {
        graph::Document document_{};
        std::vector<graph::NodeId> viewers_{};
        std::uint64_t generation_ = 0;
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
