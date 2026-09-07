#pragma once

#include <array>
#include <cmath>
#include <cstdint>

namespace rhythm::audio {
constexpr std::size_t kBandCount = 63;
// Canonical normalized spectrum: 4096-point periodic Hann FFT, 63 logarithmic
// bands from 20 Hz to 16 kHz, bounded adaptive gain. No legacy 480-bar filtering.
// Immutable per evaluation; source timestamps use PCM sample position, never UI time.
struct Features {
    std::array<float, kBandCount> mono_bands_{};
    std::array<float, kBandCount> left_bands_{};
    std::array<float, kBandCount> right_bands_{};
    std::uint64_t generation_ = 0;
    std::uint64_t end_sample_ = 0;
    std::uint64_t onset_id_ = 0;
    std::uint32_t sample_rate_ = 0;
    double center_seconds_ = 0;
    double last_onset_seconds_ = 0;
    double bpm_ = 0;
    double bpm_confidence_ = 0;
    float rms_ = 0;
    float loudness_ = 0;
    float spectral_centroid_hz_ = 0;
    float spectrum_gain_ = 1;
    float onset_strength_ = 0;
    bool valid_ = false;
    bool operator==(const Features&) const = default;
};
inline bool ValidFeatures(const Features& frame) {
    for (const auto& bands : {frame.mono_bands_, frame.left_bands_, frame.right_bands_})
        for (const auto value : bands)
            if (!std::isfinite(value) || value < 0 || value > 1) return false;
    return std::isfinite(frame.rms_) && frame.rms_ >= 0 && frame.rms_ <= 1 &&
           std::isfinite(frame.loudness_) && frame.loudness_ >= 0 && frame.loudness_ <= 1 &&
           std::isfinite(frame.center_seconds_) && frame.center_seconds_ >= 0 &&
           std::isfinite(frame.last_onset_seconds_) && frame.last_onset_seconds_ >= 0 &&
           std::isfinite(frame.bpm_) && frame.bpm_ >= 0 && frame.bpm_ <= 200 &&
           std::isfinite(frame.bpm_confidence_) && frame.bpm_confidence_ >= 0 &&
           frame.bpm_confidence_ <= 1 && std::isfinite(frame.spectral_centroid_hz_) &&
           frame.spectral_centroid_hz_ >= 0 && frame.spectral_centroid_hz_ <= 96000 &&
           std::isfinite(frame.spectrum_gain_) && frame.spectrum_gain_ >= 0.35f &&
           frame.spectrum_gain_ <= 12 && std::isfinite(frame.onset_strength_) &&
           frame.onset_strength_ >= 0 &&
           (!frame.valid_ ||
            (frame.generation_ && frame.sample_rate_ >= 8000 && frame.sample_rate_ <= 192000));
}
}  // namespace rhythm::audio
