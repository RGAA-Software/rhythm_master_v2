#pragma once

#include "rhythm/media/waveform.h"
#include "rhythm/parameters/clip_interval.h"

namespace rhythm::media {
// Immutable range-extrema index. Each display column queries logarithmically;
// large source windows and many loop repetitions cannot trigger a full rescan.
class WaveformIndex final {
   public:
    explicit WaveformIndex(const WaveformOverview& overview);
    WaveformPeak Range(double begin, double end) const;
    double Duration() const { return static_cast<double>(frames_) / 48000; }
    std::size_t BinCount() const { return count_; }

   private:
    std::array<WaveformPeak, 2 * kMaximumWaveformBins> tree_{};
    std::uint64_t frames_ = 0;
    std::uint64_t frames_per_peak_ = 0;
    std::size_t count_ = 0;
};
// Raw source peaks for a placed audio interval (Blank or Loop, no sample hold).
// Source trimming and wrapping match ClipInterval. Gain/fades are drawn by the
// UI separately; this is a source envelope, not a post-pan mixed PCM waveform.
WaveformPeak ClipWaveformPeak(const WaveformIndex& waveform,
                              const parameters::ClipInterval& interval, double begin, double end);
}  // namespace rhythm::media
