#include "rhythm/player/scene_queue.h"

#include <algorithm>
#include <limits>
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
    items_.front().state_ = ScenePreparation::kQueued;
    items_.front().error_ = PackageLoadError::kNone;
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
}  // namespace rhythm::player
