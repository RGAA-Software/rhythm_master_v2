#include "rhythm/performance/list.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>
#include <utility>

namespace rhythm::performance {
namespace {
bool ValidTitle(const std::string& title) {
    return !title.empty() && title.size() <= 512 && title.find('\0') == std::string::npos;
}
bool ValidIdentity(const std::string& identity) {
    return !identity.empty() && identity.size() <= 128 &&
           std::all_of(identity.begin(), identity.end(), [](char value) {
               return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
                      (value >= '0' && value <= '9') || value == '.' || value == '_' ||
                      value == '-' || value == '+';
           });
}
}  // namespace
bool ValidWork(const WorkReference& work) {
    if (!assets::ValidId(work.package_)) return false;
    if (work.source_ == WorkSource::kManaged)
        return work.content_id_.empty() && work.version_.empty() &&
               work.policy_ == VersionPolicy::kExact;
    return work.source_ == WorkSource::kBuiltin && ValidIdentity(work.content_id_) &&
           ValidIdentity(work.version_) &&
           (work.policy_ == VersionPolicy::kExact ||
            work.policy_ == VersionPolicy::kCurrentBuiltin);
}
bool ValidEntry(const ListEntry& entry) {
    return entry.id_ != 0 && ValidWork(entry.work_) && ValidTitle(entry.title_) &&
           std::isfinite(entry.transition_seconds_) && entry.transition_seconds_ >= 0 &&
           entry.transition_seconds_ <= 5 &&
           (entry.quantization_ == parameters::Quantization::kImmediate ||
            entry.quantization_ == parameters::Quantization::kBeat ||
            entry.quantization_ == parameters::Quantization::kBar);
}
List::List(std::string title, std::vector<ListEntry> entries, std::uint64_t last_id)
    : title_(std::move(title)), entries_(std::move(entries)), last_id_(last_id) {
    if (!ValidTitle(title_) || entries_.size() > kMaximumItems)
        throw std::invalid_argument("performance.list_limit");
    std::set<std::uint64_t> ids;
    for (const auto& entry : entries_) {
        if (!ValidEntry(entry) || !ids.insert(entry.id_).second)
            throw std::invalid_argument("performance.entry");
        last_id_ = std::max(last_id_, entry.id_);
    }
}
bool List::Rename(std::string title) {
    if (!ValidTitle(title)) return false;
    title_ = std::move(title);
    return true;
}
std::optional<std::uint64_t> List::Append(ListEntry entry) {
    if (entries_.size() == kMaximumItems || last_id_ == std::numeric_limits<std::uint64_t>::max())
        return {};
    entry.id_ = last_id_ + 1;
    if (!ValidEntry(entry)) return {};
    entries_.push_back(std::move(entry));
    return ++last_id_;
}
bool List::Replace(ListEntry entry) {
    if (!ValidEntry(entry)) return false;
    const auto found = std::find_if(entries_.begin(), entries_.end(),
                                    [&](const auto& item) { return item.id_ == entry.id_; });
    if (found == entries_.end()) return false;
    *found = std::move(entry);
    return true;
}
bool List::Remove(std::uint64_t id) {
    return std::erase_if(entries_, [&](const auto& item) { return item.id_ == id; }) != 0;
}
bool List::Move(std::uint64_t id, std::size_t position) {
    if (position >= entries_.size()) return false;
    const auto found = std::find_if(entries_.begin(), entries_.end(),
                                    [&](const auto& item) { return item.id_ == id; });
    if (found == entries_.end()) return false;
    const auto target = entries_.begin() + static_cast<std::ptrdiff_t>(position);
    if (found < target) std::rotate(found, found + 1, target + 1);
    if (found > target) std::rotate(target, found, found + 1);
    return true;
}
WorkResolution Resolve(const WorkReference& work, std::span<const WorkReference> catalog) {
    if (!ValidWork(work)) return {};
    std::optional<std::size_t> match;
    for (std::size_t index = 0; index < catalog.size(); ++index) {
        const auto& candidate = catalog[index];
        if (!ValidWork(candidate) || work.source_ != candidate.source_) continue;
        const bool same = work.source_ == WorkSource::kManaged
                                  ? work.package_ == candidate.package_
                                  : work.content_id_ == candidate.content_id_;
        if (!same) continue;
        if (match) return {ResolutionState::kAmbiguous, {}};
        match = index;
    }
    if (!match) return {};
    const auto& candidate = catalog[*match];
    if (candidate.package_ == work.package_ && candidate.version_ == work.version_)
        return {ResolutionState::kExact, match};
    if (work.policy_ == VersionPolicy::kCurrentBuiltin) return {ResolutionState::kUpdated, match};
    return {ResolutionState::kChanged, {}};
}
}  // namespace rhythm::performance
