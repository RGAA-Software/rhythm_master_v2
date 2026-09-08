#pragma once

#include <future>

#include "rhythm/assets/store.h"
#include "rhythm/foundation/blocking_executor.h"
#include "rhythm/image_shader/source.h"

namespace rhythm::shader_authoring {
struct Toolchain {
    std::filesystem::path compiler_{};
    std::filesystem::path includes_{};
    std::filesystem::path varying_{};
};
struct Request {
    Toolchain tools_{};
    std::filesystem::path assets_{};
    std::string expression_{};
};
struct Result {
    std::optional<assets::AssetRecord> asset_{};
    std::string error_{};
    std::optional<image_shader::Diagnostic> diagnostic_{};
};
// Host-thread facade. One cancellable worker owns compiler processes and I/O;
// success publishes an immutable, dual-target asset. No render/UI mutation.
class Compiler final {
   public:
    ~Compiler();
    bool Start(Request request);
    bool Busy() const { return pending_.valid(); }
    void Cancel();
    std::optional<Result> Take();

   private:
    foundation::BlockingExecutor executor_{{1, 1}};
    std::stop_source cancellation_{};
    std::future<Result> pending_{};
};
}  // namespace rhythm::shader_authoring
