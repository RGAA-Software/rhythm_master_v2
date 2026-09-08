#pragma once

#include <span>

#include "rhythm/player/package_loader.h"

namespace rhythm::player {
enum class ScenePreparation { kQueued, kLoading, kReady, kFailed };
struct SceneQueueItem {
    std::uint64_t id_ = 0;
    std::filesystem::path source_{};
    std::string title_{};
    ScenePreparation state_ = ScenePreparation::kQueued;
    PackageLoadError error_ = PackageLoadError::kNone;
};
// Host-thread FIFO. At most one prepared package or active preparation worker;
// only lightweight source metadata is retained for later entries. The loader
// owns all I/O. Call Pump(false) during a transition to avoid preparing a third
// scene. Item spans are valid only until the next mutation on this host thread.
class SceneQueue final {
   public:
    std::optional<std::uint64_t> Enqueue(std::filesystem::path source, std::string title);
    bool Remove(std::uint64_t id);
    bool Retry();
    void Clear();
    void Pump(bool can_prepare);
    std::optional<PreparedPackage> TakeReady();
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
