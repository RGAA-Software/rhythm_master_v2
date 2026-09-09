#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "rhythm/assets/types.h"
#include "rhythm/parameters/beat_grid.h"

namespace rhythm::performance {
enum class WorkSource { kBuiltin, kManaged };
enum class VersionPolicy { kExact, kCurrentBuiltin };
// Persistent identity only. Hosts resolve it to a checked package on their own
// platform; paths, prepared resources and a playing cursor never enter the list.
struct WorkReference {
    WorkSource source_ = WorkSource::kBuiltin;
    std::string content_id_{};
    std::string version_{};
    assets::AssetId package_{};
    VersionPolicy policy_ = VersionPolicy::kExact;
    bool operator==(const WorkReference&) const = default;
};
bool ValidWork(const WorkReference& work);
struct ListEntry {
    std::uint64_t id_ = 0;
    WorkReference work_{};
    std::string title_{};
    double transition_seconds_ = 1;
    parameters::Quantization quantization_ = parameters::Quantization::kImmediate;
    bool operator==(const ListEntry&) const = default;
};
bool ValidEntry(const ListEntry& entry);
// Host-thread authoring value, independent of the consuming playback queue.
// Duplicate works are allowed; entry IDs remain unique across delete/reopen/add.
class List final {
   public:
    static constexpr std::size_t kMaximumItems = 16;
    explicit List(std::string title = "Performance", std::vector<ListEntry> entries = {},
                  std::uint64_t last_id = 0);
    const std::string& Title() const { return title_; }
    std::span<const ListEntry> Entries() const { return entries_; }
    std::uint64_t LastId() const { return last_id_; }
    bool Rename(std::string title);
    // Ignores the supplied entry ID and allocates a fresh one. Rejection is atomic.
    std::optional<std::uint64_t> Append(ListEntry entry);
    bool Replace(ListEntry entry);
    bool Remove(std::uint64_t id);
    // Position is the final zero-based index, not an insertion boundary.
    bool Move(std::uint64_t id, std::size_t position);
    bool operator==(const List&) const = default;

   private:
    std::string title_{};
    std::vector<ListEntry> entries_{};
    std::uint64_t last_id_ = 0;
};
enum class ResolutionState { kExact, kUpdated, kMissing, kChanged, kAmbiguous };
struct WorkResolution {
    ResolutionState state_ = ResolutionState::kMissing;
    // Index into the caller's immutable catalog, never a borrowed object address.
    std::optional<std::size_t> catalog_index_{};
};
// Catalog contains one current revision per built-in content ID. Managed works
// resolve by exact package bytes. An invalid or duplicate match is never chosen.
WorkResolution Resolve(const WorkReference& work, std::span<const WorkReference> catalog);
}  // namespace rhythm::performance
