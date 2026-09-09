#pragma once

#include <span>

#include "rhythm/player/package_loader.h"
#include "rhythm/player/resolved_work.h"

namespace rhythm::player {
enum class ScenePreparation {
    kQueued,
    kLoading,
    kReady,
    kFailed,
    kGpuPreparing,
    kPresentable,
    kTransitioning
};
struct SceneQueueItem {
    std::uint64_t id_ = 0;
    std::filesystem::path source_{};
    std::string title_{};
    ScenePreparation state_ = ScenePreparation::kQueued;
    PackageLoadError error_ = PackageLoadError::kNone;
    storage::FileBytes bytes_{};
    std::optional<performance::ListEntry> entry_{};
    performance::ResolutionState resolution_ = performance::ResolutionState::kExact;
    std::string resolution_error_{};
    std::string preparation_error_{};
};
// Host-thread FIFO. At most one prepared package or active preparation worker;
// only lightweight source metadata is retained for later entries. The loader
// owns all I/O. Call Pump(false) during a transition to avoid preparing a third
// scene. Item spans are valid only until the next mutation on this host thread.
class SceneQueue final {
   public:
    std::optional<std::uint64_t> Enqueue(std::filesystem::path source, std::string title);
    std::optional<std::uint64_t> EnqueueBytes(storage::FileBytes source, std::string title);
    // Installs a resolved performance in one host-thread mutation. Missing rows
    // stay visible and cannot silently fall through to the next work. Never plays.
    bool ReplacePerformance(std::span<const ResolvedWork> works);
    bool Remove(std::uint64_t id);
    bool Retry();
    void Clear();
    void Pump(bool can_prepare);
    std::optional<PreparedPackage> TakeReady();
    // Transfer CPU data without removing its row. GPU failure remains retryable.
    std::optional<PreparedPackage> BeginGraphics();
    bool UpdateGraphics(std::uint64_t id, bool ready);
    bool FailGraphics(std::uint64_t id, std::string error);
    bool StartGraphics(std::uint64_t id);
    bool StartReplacement(std::uint64_t id);
    bool FinishGraphics(std::uint64_t id, bool accepted, std::string error = {});
    std::span<const SceneQueueItem> Items() const { return items_; }
    bool Busy() const { return loader_.Busy(); }
    static constexpr std::size_t kMaximumItems = 16;

   private:
    std::vector<SceneQueueItem> items_{};
    std::optional<PreparedPackage> ready_{};
    std::uint64_t next_id_ = 1;
    std::uint64_t loading_id_ = 0;
    PackageLoader loader_{};
};
}  // namespace rhythm::player
