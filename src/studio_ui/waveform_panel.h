#pragma once

#include <map>

#include "audio_clip_editor.h"
#include "rhythm/media/waveform.h"
#include "rhythm/media/waveform_cache.h"

namespace rhythm::studio {
// Draws one bounded peak envelope. Seek commits on left-button release inside
// the waveform, so dragging does not repeatedly restart exact audio decoding.
std::optional<double> DrawWaveform(const media::WaveformOverview& overview, double seconds);
class WaveformPanel final {
   public:
    std::optional<double> Draw(const std::optional<std::filesystem::path>& source, double seconds,
                               const std::map<std::string, std::string>& text);
    void Clear();
    ClipWaveforms DrawClips(const std::filesystem::path& directory,
                            std::span<const media::AudioClip> clips,
                            std::span<const assets::AssetRecord> assets,
                            const std::map<std::string, std::string>& text);
    std::size_t BinCount() const { return overview_ ? overview_->count_ : 0; }
    std::size_t ClipSourceCount() const { return clips_.ReadyCount(); }

   private:
    std::optional<media::WaveformScanner> scanner_{};
    std::optional<std::filesystem::path> source_{};
    std::optional<media::WaveformOverview> overview_{};
    std::string error_{};
    std::uint64_t generation_ = 0;
    std::uint64_t active_generation_ = 0;
    bool needs_scan_ = false;
    media::WaveformCache clips_{};
};
}  // namespace rhythm::studio
