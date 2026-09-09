#pragma once

#include <span>

#include "rhythm/media/audio_decoder.h"

namespace rhythm::media {
enum class CrossfadeCurve { kLinear, kEqualPower };
struct CrossfadeWeights {
    double previous_ = 1;
    double next_ = 0;
};
CrossfadeWeights CrossfadeAt(std::uint64_t elapsed_frames, std::uint64_t duration_frames,
                             CrossfadeCurve curve);
struct CrossfadeBlock {
    std::vector<float> samples_{};
    std::size_t clipped_samples_ = 0;
};
// Stereo/48 kHz, at most 4096 frames. An empty/shorter input is silence for the
// remaining frames. Gains are authored source gains, before the device master.
// Mix once, then clip once; reports clipped channel samples, not stereo frames.
CrossfadeBlock MixCrossfade(std::span<const float> previous, std::span<const float> next,
                            std::uint64_t elapsed_frames, std::uint64_t duration_frames,
                            CrossfadeCurve curve = CrossfadeCurve::kLinear, float previous_gain = 1,
                            float next_gain = 1);
}  // namespace rhythm::media
