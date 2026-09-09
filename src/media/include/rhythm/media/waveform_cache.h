#pragma once

#include <map>

#include "rhythm/media/waveform_index.h"

namespace rhythm::media {
// Host-thread demand/cache state; one temporary scanner publishes immutable
// envelopes. Repeated clips share asset identity. Idle caches own no workers.
class WaveformCache final {
   public:
    void Update(const std::filesystem::path& directory,
                std::span<const assets::AssetRecord> records);
    void Clear();
    void Retry();
    std::shared_ptr<const WaveformIndex> Find(const assets::AssetId& id) const;
    std::size_t ReadyCount() const;
    std::size_t FailureCount() const;
    bool Busy() const { return scanner_.has_value(); }
    static constexpr std::size_t kMaximumSources = 32;

   private:
    struct Entry {
        assets::AssetRecord record_{};
        std::shared_ptr<const WaveformIndex> waveform_{};
        bool failed_ = false;
    };
    void Poll();
    std::filesystem::path directory_{};
    std::map<std::string, Entry> entries_{};
    std::optional<WaveformScanner> scanner_{};
    assets::AssetRecord active_record_{};
    std::filesystem::path active_directory_{};
    bool active_cancelled_ = false;
};
}  // namespace rhythm::media
