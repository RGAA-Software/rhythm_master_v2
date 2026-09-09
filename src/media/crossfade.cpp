#include "rhythm/media/crossfade.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace rhythm::media {
CrossfadeWeights CrossfadeAt(std::uint64_t elapsed_frames, std::uint64_t duration_frames,
                             CrossfadeCurve curve) {
    if (duration_frames > 5 * kAudioSampleRate ||
        (curve != CrossfadeCurve::kLinear && curve != CrossfadeCurve::kEqualPower))
        throw std::invalid_argument("audio.crossfade_settings");
    if (elapsed_frames >= duration_frames) return {0, 1};
    if (!elapsed_frames) return {1, 0};
    const double fraction = double(elapsed_frames) / double(duration_frames);
    if (curve == CrossfadeCurve::kLinear) return {1 - fraction, fraction};
    const double angle = fraction * std::numbers::pi / 2;
    return {std::cos(angle), std::sin(angle)};
}
CrossfadeBlock MixCrossfade(std::span<const float> previous, std::span<const float> next,
                            std::uint64_t elapsed_frames, std::uint64_t duration_frames,
                            CrossfadeCurve curve, float previous_gain, float next_gain) {
    if (previous.size() % kAudioChannels || next.size() % kAudioChannels ||
        previous.size() > kAudioBlockFrames * kAudioChannels ||
        next.size() > kAudioBlockFrames * kAudioChannels || !std::isfinite(previous_gain) ||
        !std::isfinite(next_gain) || previous_gain < 0 || previous_gain > 1 || next_gain < 0 ||
        next_gain > 1)
        throw std::invalid_argument("audio.crossfade_input");
    (void)CrossfadeAt(elapsed_frames, duration_frames, curve);
    CrossfadeBlock result;
    result.samples_.resize(std::max(previous.size(), next.size()));
    for (std::size_t index = 0; index < result.samples_.size(); index += kAudioChannels) {
        const auto offset = index / kAudioChannels;
        const auto elapsed =
                elapsed_frames >= duration_frames
                        ? duration_frames
                        : elapsed_frames +
                                  std::min<std::uint64_t>(offset, duration_frames - elapsed_frames);
        const auto weights = CrossfadeAt(elapsed, duration_frames, curve);
        for (std::size_t channel = 0; channel < kAudioChannels; ++channel) {
            const auto sample = index + channel;
            const double a = sample < previous.size() ? previous[sample] : 0;
            const double b = sample < next.size() ? next[sample] : 0;
            if (!std::isfinite(a) || !std::isfinite(b))
                throw std::invalid_argument("audio.invalid_sample");
            const auto mixed =
                    a * weights.previous_ * previous_gain + b * weights.next_ * next_gain;
            result.clipped_samples_ += mixed < -1 || mixed > 1;
            result.samples_[sample] = static_cast<float>(std::clamp(mixed, -1.0, 1.0));
        }
    }
    return result;
}
}  // namespace rhythm::media
