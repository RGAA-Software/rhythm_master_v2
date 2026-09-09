#include "rhythm/player/performance_program.h"

#include <chrono>
#include <cmath>
#include <stdexcept>
#include <utility>

#include "rhythm/assets/store.h"
#include "rhythm/project/package.h"
#include "rhythm/project/performance_store.h"

namespace rhythm::player {
PerformanceProgram::PerformanceProgram(std::filesystem::path directory,
                                       std::vector<performance::WorkReference> catalog,
                                       BuiltinWorkReader builtin_reader)
    : directory_(std::move(directory)),
      catalog_(std::move(catalog)),
      builtin_reader_(std::move(builtin_reader)) {
    if (catalog_.size() > 1024) throw std::length_error("performance.catalog_limit");
    for (const auto& work : catalog_)
        if (!performance::ValidWork(work) || work.source_ != performance::WorkSource::kBuiltin)
            throw std::invalid_argument("performance.catalog");
}
PerformanceProgram::~PerformanceProgram() {
    Cancel();
    executor_.RequestStop(foundation::ShutdownMode::kDrain);
    executor_.Join();
}
bool PerformanceProgram::Edit(performance::List draft) {
    if (Busy()) return false;
    if (draft_ != draft) {
        draft_ = std::move(draft);
        status_.dirty_ = true;
        status_.error_.clear();
        resolved_.reset();
    }
    return true;
}
bool PerformanceProgram::Start(ProgramOperation operation,
                               std::function<Result(std::stop_token)> work) {
    if (Busy()) return false;
    cancellation_ = {};
    auto task = std::make_shared<std::packaged_task<Result()>>(
            [work = std::move(work), stop = cancellation_.get_token()] {
                try {
                    if (stop.stop_requested()) throw std::runtime_error("performance.cancelled");
                    return work(stop);
                } catch (const std::exception& error) {
                    Result result;
                    result.error_ = error.what();
                    return result;
                }
            });
    auto completion = task->get_future();
    if (executor_.TryPost([task] { (*task)(); }) != foundation::SubmitResult::kAccepted)
        return false;
    pending_ = std::move(completion);
    status_.operation_ = operation;
    status_.error_.clear();
    resolved_.reset();
    return true;
}
bool PerformanceProgram::Load() {
    return Start(ProgramOperation::kLoad, [directory = directory_](std::stop_token) {
        Result result;
        if (std::filesystem::exists(directory / "list.json")) {
            result.list_ = project::LoadPerformanceList(directory);
            result.saved_ = true;
        } else {
            result.list_ = performance::List{};
        }
        return result;
    });
}
bool PerformanceProgram::Save() {
    return Start(ProgramOperation::kSave,
                 [directory = directory_, draft = draft_](std::stop_token) {
                     project::SavePerformanceList(directory, draft);
                     Result result;
                     result.saved_ = true;
                     return result;
                 });
}
bool PerformanceProgram::Import(std::filesystem::path source, double duration,
                                parameters::Quantization mode,
                                std::optional<performance::WorkReference> expected) {
    if (Busy() || draft_.Entries().size() >= performance::List::kMaximumItems ||
        draft_.LastId() == UINT64_MAX || !std::isfinite(duration) || duration < 0 || duration > 5 ||
        (mode != parameters::Quantization::kImmediate && mode != parameters::Quantization::kBeat &&
         mode != parameters::Quantization::kBar))
        return false;
    if (!Start(ProgramOperation::kImport, [directory = directory_, source = std::move(source),
                                           expected = std::move(expected)](std::stop_token stop) {
            Result result;
            result.work_ = WorkLibrary(directory / "works").Import(source, expected, stop);
            return result;
        }))
        return false;
    import_duration_ = duration;
    import_mode_ = mode;
    return true;
}
bool PerformanceProgram::Resolve() {
    return Start(ProgramOperation::kResolve, [directory = directory_, draft = draft_,
                                              catalog = catalog_,
                                              reader = builtin_reader_](std::stop_token stop) {
        Result result;
        result.resolved_.emplace();
        WorkLibrary library(directory / "works");
        for (const auto& entry : draft.Entries()) {
            if (stop.stop_requested()) throw std::runtime_error("performance.cancelled");
            ResolvedWork resolved{entry};
            try {
                if (entry.work_.source_ == performance::WorkSource::kManaged) {
                    resolved.bytes_ = library.Open(entry.work_, stop);
                    resolved.state_ = performance::ResolutionState::kExact;
                } else {
                    const auto resolution = performance::Resolve(entry.work_, catalog);
                    resolved.state_ = resolution.state_;
                    if (resolution.catalog_index_) {
                        auto effective = catalog[*resolution.catalog_index_];
                        effective.policy_ = entry.work_.policy_;
                        resolved.entry_.work_ = effective;
                        if (!reader) throw std::runtime_error("performance.builtin_reader");
                        resolved.bytes_ = reader(effective, stop);
                        const assets::AssetRecord asset{effective.package_, resolved.bytes_.Size(),
                                                        "application/vnd.rhythm.package"};
                        if (!resolved.bytes_.Valid() || !resolved.bytes_.Size() ||
                            resolved.bytes_.Size() > project::kMaximumFilePackageBytes ||
                            !assets::VerifyFile(asset, resolved.bytes_, stop))
                            throw std::runtime_error("performance.package_hash");
                    }
                }
            } catch (const std::exception& error) {
                if (stop.stop_requested()) throw;
                resolved.bytes_ = {};
                resolved.error_ = error.what();
            }
            result.resolved_->push_back(std::move(resolved));
        }
        return result;
    });
}
void PerformanceProgram::Cancel() { cancellation_.request_stop(); }
void PerformanceProgram::Pump() {
    if (!Busy() || pending_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
    auto result = pending_.get();
    const auto operation = std::exchange(status_.operation_, ProgramOperation::kIdle);
    // Saving may already have atomically committed; always report that truth.
    if (cancellation_.stop_requested() && operation != ProgramOperation::kSave) {
        status_.error_ = "performance.cancelled";
        return;
    }
    if (!result.error_.empty()) {
        status_.error_ = std::move(result.error_);
        return;
    }
    if (operation == ProgramOperation::kLoad) {
        draft_ = std::move(*result.list_);
        status_.dirty_ = false;
        status_.saved_ = result.saved_;
    } else if (operation == ProgramOperation::kSave) {
        status_.dirty_ = false;
        status_.saved_ = true;
    } else if (operation == ProgramOperation::kImport) {
        if (!draft_.Append({0, result.work_->reference_, result.work_->title_, import_duration_,
                            import_mode_}))
            status_.error_ = "performance.entry_limit";
        else
            status_.dirty_ = true;
    } else if (operation == ProgramOperation::kResolve) {
        resolved_ = std::move(result.resolved_);
    }
}
std::optional<std::vector<ResolvedWork>> PerformanceProgram::TakeResolved() {
    return std::exchange(resolved_, {});
}
}  // namespace rhythm::player
