#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>

#include "rhythm/assets/types.h"

namespace rhythm::media {
inline constexpr std::size_t kMaximumWaveformBins = 4096;
struct WaveformPeak {
    float minimum_ = 0;
    float maximum_ = 0;
};
// A zero-based 48 kHz stereo peak envelope, including the zero baseline. Combining
// both channels preserves anti-phase/transient peaks; it is not a mono mix.
struct WaveformOverview {
    std::array<WaveformPeak, kMaximumWaveformBins> peaks_{};
    std::size_t count_ = 0;
    std::uint64_t frames_per_peak_ = 256;
    std::uint64_t frames_ = 0;
    double Duration() const { return static_cast<double>(frames_) / 48000; }
};
// Serialized worker-owned accumulator. Adjacent complete bins merge when full;
// memory stays constant and the last partial bin retains its exact time extent.
class WaveformAccumulator final {
   public:
    void Append(std::span<const float> stereo, std::uint64_t first_frame);
    const WaveformOverview& Overview() const { return overview_; }

   private:
    WaveformOverview overview_{};
};
struct WaveformResult {
    std::optional<WaveformOverview> overview_{};
    std::string error_{};
};
// Host-thread admission/polling, one worker and one result. Reads only on that
// worker, through the existing local audio decoder. Cancel never joins; owners
// discard canceled results. A scan has a 256 MiB source and 120-second work limit.
class WaveformScanner final {
   public:
    WaveformScanner();
    ~WaveformScanner();
    WaveformScanner(const WaveformScanner&) = delete;
    WaveformScanner& operator=(const WaveformScanner&) = delete;
    bool Start(std::filesystem::path source);
    // The asset directory and record are values; verification and opening occur
    // on the scanner's worker, through the existing immutable asset store.
    bool StartAsset(std::filesystem::path directory, assets::AssetRecord asset);
    bool Busy() const;
    void Cancel();
    std::optional<WaveformResult> Take();
    double SecondsScanned() const;

   private:
    class Impl;
    std::unique_ptr<Impl> impl_{};
};
}  // namespace rhythm::media
