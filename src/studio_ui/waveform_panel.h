#pragma once

#include <map>

#include "rhythm/media/waveform.h"

namespace rhythm::studio {
// Draws one bounded peak envelope. Seek commits on left-button release inside
// the waveform, so dragging does not repeatedly restart exact audio decoding.
std::optional<double> DrawWaveform(const media::WaveformOverview& overview, double seconds);
class WaveformPanel final {
   public:
    std::optional<double> Draw(const std::optional<std::filesystem::path>& source, double seconds,
                               const std::map<std::string, std::string>& text);
    void Clear();
    std::size_t BinCount() const { return overview_ ? overview_->count_ : 0; }

   private:
    std::optional<media::WaveformScanner> scanner_{};
    std::optional<std::filesystem::path> source_{};
    std::optional<media::WaveformOverview> overview_{};
    std::string error_{};
    std::uint64_t generation_ = 0;
    std::uint64_t active_generation_ = 0;
    bool needs_scan_ = false;
};
}  // namespace rhythm::studio
