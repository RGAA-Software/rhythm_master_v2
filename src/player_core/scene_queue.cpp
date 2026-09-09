#include "rhythm/player/scene_queue.h"

#include <algorithm>
#include <limits>
#include <set>
#include <utility>

namespace rhythm::player {
std::optional<std::uint64_t> SceneQueue::Enqueue(std::filesystem::path source, std::string title) {
    if (source.empty() || title.empty() || title.size() > 512 ||
        title.find('\0') != std::string::npos || items_.size() >= kMaximumItems ||
        next_id_ == std::numeric_limits<std::uint64_t>::max())
        return {};
    const auto id = next_id_++;
    items_.push_back({id, std::move(source), std::move(title)});
    return id;
}
std::optional<std::uint64_t> SceneQueue::EnqueueBytes(storage::FileBytes source,
                                                      std::string title) {
    if (!source.Valid() || !source.Size() || source.Size() > project::kMaximumFilePackageBytes ||
        title.empty() || title.size() > 512 || title.find('\0') != std::string::npos ||
        items_.size() >= kMaximumItems || next_id_ == std::numeric_limits<std::uint64_t>::max())
        return {};
    const auto id = next_id_++;
    items_.push_back({id,
                      {},
                      std::move(title),
                      ScenePreparation::kQueued,
                      PackageLoadError::kNone,
                      std::move(source)});
    return id;
}
bool SceneQueue::Remove(std::uint64_t id) {
    const auto found = std::find_if(items_.begin(), items_.end(),
                                    [&](const auto& item) { return item.id_ == id; });
    if (found == items_.end()) return false;
    if (found == items_.begin()) {
        ready_.reset();
        if (loading_id_ == id) loader_.Cancel();
    }
    items_.erase(found);
    return true;
}
void SceneQueue::Clear() {
    loader_.Cancel();
    items_.clear();
    ready_.reset();
}
bool SceneQueue::Retry() {
    if (items_.empty() || items_.front().state_ != ScenePreparation::kFailed) return false;
    if (items_.front().entry_ && !items_.front().bytes_.Valid()) return false;
    items_.front().state_ = ScenePreparation::kQueued;
    items_.front().error_ = PackageLoadError::kNone;
    items_.front().preparation_error_.clear();
    return true;
}
void SceneQueue::Pump(bool can_prepare) {
    if (auto completion = loader_.Take()) {
        // Deleting/clearing a loading row does not synchronously join its worker.
        // Its late result is discarded before another row can start preparation.
        if (!items_.empty() && items_.front().id_ == loading_id_) {
            auto& item = items_.front();
            if (completion->package_) {
                ready_ = std::move(completion->package_);
                item.state_ = ScenePreparation::kReady;
            } else {
                item.state_ = ScenePreparation::kFailed;
                item.error_ = completion->error_;
            }
        }
        loading_id_ = 0;
    }
    if (!can_prepare || loader_.Busy() || ready_ || items_.empty() ||
        items_.front().state_ != ScenePreparation::kQueued)
        return;
    auto& item = items_.front();
    if (item.bytes_.Valid() ? loader_.StartBytes(item.bytes_) : loader_.StartFile(item.source_)) {
        item.state_ = ScenePreparation::kLoading;
        loading_id_ = item.id_;
    } else {
        item.state_ = ScenePreparation::kFailed;
        item.error_ = PackageLoadError::kRead;
    }
}
std::optional<PreparedPackage> SceneQueue::TakeReady() {
    if (items_.empty() || items_.front().state_ != ScenePreparation::kReady || !ready_) return {};
    auto result = std::exchange(ready_, std::nullopt);
    items_.erase(items_.begin());
    return result;
}
std::optional<PreparedPackage> SceneQueue::BeginGraphics() {
    if (items_.empty() || items_.front().state_ != ScenePreparation::kReady || !ready_) return {};
    items_.front().state_ = ScenePreparation::kGpuPreparing;
    return std::exchange(ready_, std::nullopt);
}
bool SceneQueue::UpdateGraphics(std::uint64_t id, bool ready) {
    if (items_.empty() || items_.front().id_ != id ||
        (items_.front().state_ != ScenePreparation::kGpuPreparing &&
         items_.front().state_ != ScenePreparation::kPresentable))
        return false;
    items_.front().state_ =
            ready ? ScenePreparation::kPresentable : ScenePreparation::kGpuPreparing;
    return true;
}
bool SceneQueue::FailGraphics(std::uint64_t id, std::string error) {
    if (!UpdateGraphics(id, false)) return false;
    items_.front().state_ = ScenePreparation::kFailed;
    items_.front().preparation_error_ = std::move(error);
    return true;
}
bool SceneQueue::StartGraphics(std::uint64_t id) {
    if (items_.empty() || items_.front().id_ != id ||
        items_.front().state_ != ScenePreparation::kPresentable)
        return false;
    items_.front().state_ = ScenePreparation::kTransitioning;
    return true;
}
bool SceneQueue::FinishGraphics(std::uint64_t id, bool accepted, std::string error) {
    if (items_.empty() || items_.front().id_ != id ||
        items_.front().state_ != ScenePreparation::kTransitioning)
        return false;
    if (accepted) {
        items_.erase(items_.begin());
    } else {
        items_.front().state_ = ScenePreparation::kFailed;
        items_.front().preparation_error_ = std::move(error);
    }
    return true;
}
bool SceneQueue::ReplacePerformance(std::span<const ResolvedWork> works) {
    if (works.size() > kMaximumItems ||
        next_id_ > std::numeric_limits<std::uint64_t>::max() - works.size())
        return false;
    std::vector<SceneQueueItem> replacement;
    std::set<std::uint64_t> entries;
    auto next = next_id_;
    for (const auto& work : works) {
        if (!performance::ValidEntry(work.entry_) || !entries.insert(work.entry_.id_).second)
            return false;
        if (work.bytes_.Valid() && (!work.error_.empty() || !work.bytes_.Size() ||
                                    work.bytes_.Size() > project::kMaximumFilePackageBytes ||
                                    (work.state_ != performance::ResolutionState::kExact &&
                                     work.state_ != performance::ResolutionState::kUpdated)))
            return false;
        SceneQueueItem item;
        item.id_ = next++;
        item.title_ = work.entry_.title_;
        item.bytes_ = work.bytes_;
        item.entry_ = work.entry_;
        item.resolution_ = work.state_;
        item.resolution_error_ = work.error_;
        if (!item.bytes_.Valid()) {
            item.state_ = ScenePreparation::kFailed;
            item.error_ = PackageLoadError::kRead;
        }
        replacement.push_back(std::move(item));
    }
    Clear();
    items_ = std::move(replacement);
    next_id_ = next;
    return true;
}
}  // namespace rhythm::player
