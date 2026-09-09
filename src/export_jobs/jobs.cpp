#include "rhythm/export/jobs.h"

#include <chrono>
#include <mutex>
#include <thread>

#include "export_process.h"
#include "job_files.h"
#include "rhythm/foundation/blocking_executor.h"
#include "rhythm/project/store.h"
#include "rhythm/storage/atomic_file.h"
#include "workspace.h"

namespace rhythm::exporting {
namespace {
bool Busy(JobState state) {
    return state == JobState::kPreparing || state == JobState::kRendering ||
           state == JobState::kPublishing;
}
}  // namespace
class ExportJobs::Impl final {
   public:
    ~Impl() {
        Cancel();
        executor_.RequestStop(foundation::ShutdownMode::kDrain);
        executor_.Join();
    }
    bool Start(std::filesystem::path executable, editor::Snapshot snapshot,
               std::filesystem::path assets, ExportSettings settings,
               std::filesystem::path destination) {
        std::lock_guard lock(mutex_);
        if (Busy(snapshot_.state_)) return false;
        cancel_ = {};
        snapshot_ = {JobState::kPreparing,
                     {0, settings.frames_, 0},
                     destination,
                     {},
                     JobPhase::kPreparing};
        const auto stop = cancel_.get_token();
        const auto submitted = executor_.TryPost([this, executable = std::move(executable),
                                                  project = std::move(snapshot),
                                                  assets = std::move(assets),
                                                  settings = std::move(settings),
                                                  destination = std::move(destination), stop] {
            try {
                Execute(executable, project, assets, settings, destination, stop);
                SetState(JobState::kComplete, JobPhase::kComplete);
            } catch (const std::exception& error) {
                std::lock_guard completed_lock(mutex_);
                snapshot_.state_ = stop.stop_requested() ? JobState::kCanceled : JobState::kFailed;
                snapshot_.error_ = snapshot_.state_ == JobState::kFailed ? error.what() : "";
            }
        });
        if (submitted != foundation::SubmitResult::kAccepted) {
            snapshot_.state_ = JobState::kFailed;
            snapshot_.error_ = "export.worker_unavailable";
            return false;
        }
        return true;
    }
    void Cancel() {
        std::lock_guard lock(mutex_);
        if (snapshot_.state_ == JobState::kPreparing || snapshot_.state_ == JobState::kRendering)
            cancel_.request_stop();
    }
    JobSnapshot Snapshot() const {
        std::lock_guard lock(mutex_);
        return snapshot_;
    }

   private:
    void SetState(JobState state, JobPhase phase) {
        std::lock_guard lock(mutex_);
        snapshot_.state_ = state;
        snapshot_.phase_ = phase;
    }
    void Execute(const std::filesystem::path& executable, const editor::Snapshot& project,
                 const std::filesystem::path& assets, const ExportSettings& settings,
                 const std::filesystem::path& destination, std::stop_token stop) {
        if (stop.stop_requested()) throw std::runtime_error("export.canceled");
        if (destination.extension() != ".mp4") throw std::invalid_argument("export.mp4_extension");
        detail::Workspace workspace(destination);
        const auto& directory = workspace.Directory();
        project::PublishSnapshot(directory / "source.rhythmpack", project, assets);
        if (settings.music_) workspace.CopyMusic(*settings.music_, stop);
        if (stop.stop_requested()) throw std::runtime_error("export.canceled");
        detail::WriteRequest(directory, settings);
        SetState(JobState::kRendering, JobPhase::kLaunching);
        {
            detail::ExportProcess process(executable, directory);
            SetState(JobState::kRendering, JobPhase::kRendering);
            auto advanced = std::chrono::steady_clock::now();
            std::uint64_t completed = 0;
            while (!process.Done()) {
                if (stop.stop_requested()) throw std::runtime_error("export.canceled");
                if (const auto progress = detail::ReadProgress(directory)) {
                    if (progress->completed_frames_ < completed ||
                        progress->total_frames_ != settings.frames_)
                        throw std::runtime_error("export.job_progress");
                    if (progress->completed_frames_ != completed)
                        advanced = std::chrono::steady_clock::now();
                    completed = progress->completed_frames_;
                    std::lock_guard lock(mutex_);
                    snapshot_.progress_ = *progress;
                    if (completed == settings.frames_) snapshot_.phase_ = JobPhase::kFinalizing;
                }
                if (std::chrono::steady_clock::now() - advanced > std::chrono::seconds(90))
                    throw std::runtime_error("export.worker_timeout");
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
        const auto result = detail::ReadResult(directory);
        if (!result.success_) throw std::runtime_error(result.error_);
        const auto final = detail::ReadProgress(directory);
        if (!final || final->completed_frames_ != settings.frames_ ||
            final->total_frames_ != settings.frames_)
            throw std::runtime_error("export.incomplete_result");
        {
            // Cancellation is accepted through rendering. Publication is the
            // commit point; a late Cancel cannot relabel a published file.
            std::lock_guard lock(mutex_);
            if (stop.stop_requested()) throw std::runtime_error("export.canceled");
            snapshot_.state_ = JobState::kPublishing;
            snapshot_.phase_ = JobPhase::kPublishing;
            snapshot_.progress_ = *final;
        }
        storage::PublishNew(directory / "output.mp4", destination);
    }
    mutable std::mutex mutex_{};
    JobSnapshot snapshot_{};
    std::stop_source cancel_{};
    foundation::BlockingExecutor executor_{{1, 1}};
};
ExportJobs::ExportJobs() : impl_(std::make_unique<Impl>()) {}
ExportJobs::~ExportJobs() = default;
bool ExportJobs::Start(std::filesystem::path executable, editor::Snapshot snapshot,
                       std::filesystem::path assets, ExportSettings settings,
                       std::filesystem::path destination) {
    return impl_->Start(std::move(executable), std::move(snapshot), std::move(assets),
                        std::move(settings), std::move(destination));
}
void ExportJobs::Cancel() { impl_->Cancel(); }
JobSnapshot ExportJobs::Snapshot() const { return impl_->Snapshot(); }
}  // namespace rhythm::exporting
