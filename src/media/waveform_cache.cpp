#include "rhythm/media/waveform_cache.h"

#include <algorithm>
#include <set>
#include <stdexcept>

namespace rhythm::media {
void WaveformCache::Poll() {
    if (!scanner_) return;
    if (auto result = scanner_->Take()) {
        const auto found = entries_.find(active_record_.id_.sha256_);
        if (!active_cancelled_ && directory_ == active_directory_ && found != entries_.end() &&
            found->second.record_ == active_record_) {
            auto& entry = found->second;
            if (result->overview_)
                entry.waveform_ = std::make_shared<const WaveformIndex>(*result->overview_);
            else
                entry.failed_ = true;
        }
        scanner_.reset();
        active_record_ = {};
        active_directory_.clear();
    }
}
void WaveformCache::Clear() {
    entries_.clear();
    directory_.clear();
    if (scanner_) {
        active_cancelled_ = true;
        scanner_->Cancel();
    }
    Poll();
}
void WaveformCache::Retry() {
    for (auto& [id, entry] : entries_) entry.failed_ = false;
}
void WaveformCache::Update(const std::filesystem::path& directory,
                           std::span<const assets::AssetRecord> records) {
    if (records.size() > kMaximumSources) throw std::length_error("waveform.source_budget");
    if (directory != directory_) {
        Clear();
        directory_ = directory;
    }
    std::set<std::string> requested;
    for (const auto& record : records) {
        if (!assets::ValidId(record.id_)) throw std::invalid_argument("waveform.asset");
        const auto& id = record.id_.sha256_;
        if (!requested.insert(id).second) continue;
        const auto found = entries_.find(id);
        if (found == entries_.end() || found->second.record_ != record)
            entries_[id] = Entry{record};
    }
    std::erase_if(entries_, [&](const auto& entry) { return !requested.contains(entry.first); });
    if (scanner_) {
        const auto found = entries_.find(active_record_.id_.sha256_);
        if (directory_ != active_directory_ || found == entries_.end() ||
            found->second.record_ != active_record_) {
            active_cancelled_ = true;
            scanner_->Cancel();
        }
    }
    Poll();
    if (scanner_) return;
    const auto pending = std::find_if(entries_.begin(), entries_.end(), [](const auto& item) {
        return !item.second.waveform_ && !item.second.failed_;
    });
    if (pending == entries_.end()) return;
    try {
        scanner_.emplace();
        active_cancelled_ = false;
        active_record_ = pending->second.record_;
        active_directory_ = directory_;
        if (!scanner_->StartAsset(directory_, active_record_)) {
            scanner_.reset();
            pending->second.failed_ = true;
        }
    } catch (const std::exception&) {
        scanner_.reset();
        pending->second.failed_ = true;
    }
}
std::shared_ptr<const WaveformIndex> WaveformCache::Find(const assets::AssetId& id) const {
    const auto found = entries_.find(id.sha256_);
    return found == entries_.end() ? std::shared_ptr<const WaveformIndex>{}
                                   : found->second.waveform_;
}
std::size_t WaveformCache::ReadyCount() const {
    return std::count_if(entries_.begin(), entries_.end(),
                         [](const auto& item) { return bool(item.second.waveform_); });
}
std::size_t WaveformCache::FailureCount() const {
    return std::count_if(entries_.begin(), entries_.end(),
                         [](const auto& item) { return item.second.failed_; });
}
}  // namespace rhythm::media
