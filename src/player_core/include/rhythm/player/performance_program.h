#pragma once

#include <functional>
#include <future>

#include "rhythm/foundation/blocking_executor.h"
#include "rhythm/player/resolved_work.h"
#include "rhythm/player/work_library.h"

namespace rhythm::player {
using BuiltinWorkReader =
        std::function<storage::FileBytes(const performance::WorkReference&, std::stop_token)>;
enum class ProgramOperation { kIdle, kLoad, kSave, kImport, kResolve };
struct ProgramStatus {
    ProgramOperation operation_ = ProgramOperation::kIdle;
    std::string error_{};
    bool dirty_ = false;
    bool saved_ = false;
};
// Host-thread editable program plus one bounded worker for persistence/import/
// resolution. Workers publish values; only Pump applies them. Saved programs are
// not consumed by playback, and resolving does not automatically switch scenes.
class PerformanceProgram final {
   public:
    PerformanceProgram(std::filesystem::path directory,
                       std::vector<performance::WorkReference> catalog,
                       BuiltinWorkReader builtin_reader);
    ~PerformanceProgram();
    const performance::List& Draft() const { return draft_; }
    const ProgramStatus& Status() const { return status_; }
    bool Busy() const { return pending_.valid(); }
    bool Edit(performance::List draft);
    bool Load();
    bool Save();
    bool Import(std::filesystem::path source, double duration, parameters::Quantization mode,
                std::optional<performance::WorkReference> expected = {});
    bool Resolve();
    void Cancel();
    void Pump();
    std::optional<std::vector<ResolvedWork>> TakeResolved();

   private:
    struct Result {
        std::optional<performance::List> list_{};
        std::optional<StoredWork> work_{};
        std::optional<std::vector<ResolvedWork>> resolved_{};
        std::string error_{};
        bool saved_ = false;
    };
    bool Start(ProgramOperation operation, std::function<Result(std::stop_token)> work);
    std::filesystem::path directory_{};
    std::vector<performance::WorkReference> catalog_{};
    BuiltinWorkReader builtin_reader_{};
    performance::List draft_{};
    ProgramStatus status_{};
    double import_duration_ = 1;
    parameters::Quantization import_mode_ = parameters::Quantization::kImmediate;
    std::optional<std::vector<ResolvedWork>> resolved_{};
    foundation::BlockingExecutor executor_{{1, 1}};
    std::future<Result> pending_{};
    std::stop_source cancellation_{};
};
}  // namespace rhythm::player
