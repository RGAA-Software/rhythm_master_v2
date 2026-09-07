#pragma once

#include <miniz.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <vector>

namespace rhythm::project::detail {
// Private allocator for untrusted ZIP metadata. Owns every allocation through
// values; raw callback addresses are borrowed by miniz only until its Free call.
// Reallocation also fits within this live-byte budget (including the old block).
class ArchiveMemory final {
   public:
    static constexpr std::size_t kMaximumBytes = 2 * 1024 * 1024;
    void Bind(mz_zip_archive& archive) {
        archive.m_pAlloc_opaque = this;
        archive.m_pAlloc = Allocate;
        archive.m_pRealloc = Reallocate;
        archive.m_pFree = Free;
    }
    std::size_t PeakBytes() const { return peak_bytes_; }

   private:
    struct Allocation {
        std::vector<std::max_align_t> storage_{};
        std::size_t bytes_ = 0;
    };
    static void* Allocate(void* opaque, std::size_t count, std::size_t size) noexcept {
        auto& owner = *static_cast<ArchiveMemory*>(opaque);
        if (!size || !count || count > kMaximumBytes / size || owner.allocations_.size() >= 128)
            return nullptr;
        const auto requested = count * size;
        const auto words = (requested + sizeof(std::max_align_t) - 1) / sizeof(std::max_align_t);
        const auto bytes = words * sizeof(std::max_align_t);
        if (bytes > kMaximumBytes - owner.live_bytes_) return nullptr;
        try {
            Allocation allocation{std::vector<std::max_align_t>(words), bytes};
            const auto address = reinterpret_cast<std::uintptr_t>(allocation.storage_.data());
            const auto [entry, inserted] =
                    owner.allocations_.emplace(address, std::move(allocation));
            if (!inserted) return nullptr;
            owner.live_bytes_ += bytes;
            owner.peak_bytes_ = std::max(owner.peak_bytes_, owner.live_bytes_);
            return entry->second.storage_.data();
        } catch (...) {
            return nullptr;
        }
    }
    static void Free(void* opaque, void* address) noexcept {
        if (!address) return;
        auto& owner = *static_cast<ArchiveMemory*>(opaque);
        const auto entry = owner.allocations_.find(reinterpret_cast<std::uintptr_t>(address));
        if (entry == owner.allocations_.end()) return;
        owner.live_bytes_ -= entry->second.bytes_;
        owner.allocations_.erase(entry);
    }
    static void* Reallocate(void* opaque, void* address, std::size_t count,
                            std::size_t size) noexcept {
        if (!address) return Allocate(opaque, count, size);
        if (!size || !count) {
            Free(opaque, address);
            return nullptr;
        }
        auto& owner = *static_cast<ArchiveMemory*>(opaque);
        const auto entry = owner.allocations_.find(reinterpret_cast<std::uintptr_t>(address));
        if (entry == owner.allocations_.end() || count > kMaximumBytes / size) return nullptr;
        const auto old_bytes = entry->second.bytes_;
        auto* replacement = Allocate(opaque, count, size);
        if (!replacement) return nullptr;
        std::memcpy(replacement, address, std::min(old_bytes, count * size));
        Free(opaque, address);
        return replacement;
    }
    std::map<std::uintptr_t, Allocation> allocations_{};
    std::size_t live_bytes_ = 0;
    std::size_t peak_bytes_ = 0;
};
}  // namespace rhythm::project::detail
