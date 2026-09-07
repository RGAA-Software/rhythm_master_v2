#pragma once

#include "rhythm/editor/history.h"
#include "rhythm/export/settings.h"

namespace rhythm::exporting {
enum class JobState { kIdle, kPreparing, kRendering, kPublishing, kComplete, kCanceled, kFailed };
struct JobSnapshot {
    JobState state_ = JobState::kIdle;
    ExportProgress progress_{};
    std::filesystem::path output_{};
    std::string error_{};
};
// Host-thread controller; one bounded worker prepares an immutable project/music
// snapshot, owns an isolated rendering process and publishes completed output.
class ExportJobs final {
   public:
    ExportJobs();
    ~ExportJobs();
    ExportJobs(const ExportJobs&) = delete;
    ExportJobs& operator=(const ExportJobs&) = delete;
    bool Start(std::filesystem::path executable, editor::Snapshot snapshot,
               std::filesystem::path assets, ExportSettings settings,
               std::filesystem::path destination);
    void Cancel();
    JobSnapshot Snapshot() const;

   private:
    class Impl;
    std::unique_ptr<Impl> impl_{};
};
// The executable's offline entry branch calls this before creating Studio.
int RunExportJob(const std::filesystem::path& directory);
}  // namespace rhythm::exporting
