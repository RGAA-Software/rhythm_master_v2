#include "rhythm/media/waveform_index.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace rhythm::media {
namespace {
WaveformPeak Merge(WaveformPeak a, WaveformPeak b) {
    return {std::min(a.minimum_, b.minimum_), std::max(a.maximum_, b.maximum_)};
}
}  // namespace
WaveformIndex::WaveformIndex(const WaveformOverview& overview)
    : frames_(overview.frames_),
      frames_per_peak_(overview.frames_per_peak_),
      count_(overview.count_) {
    if (!frames_ || !frames_per_peak_ || !count_ || count_ > kMaximumWaveformBins ||
        (frames_ - 1) / frames_per_peak_ + 1 != count_)
        throw std::invalid_argument("waveform.invalid");
    for (std::size_t index = 0; index < count_; ++index) {
        const auto peak = overview.peaks_[index];
        if (!std::isfinite(peak.minimum_) || !std::isfinite(peak.maximum_) || peak.minimum_ < -1 ||
            peak.minimum_ > 0 || peak.maximum_ < 0 || peak.maximum_ > 1)
            throw std::invalid_argument("waveform.invalid");
        tree_[kMaximumWaveformBins + index] = peak;
    }
    for (auto index = kMaximumWaveformBins - 1; index > 0; --index)
        tree_[index] = Merge(tree_[index * 2], tree_[index * 2 + 1]);
}
WaveformPeak WaveformIndex::Range(double begin, double end) const {
    if (!std::isfinite(begin) || !std::isfinite(end)) throw std::invalid_argument("waveform.range");
    begin = std::clamp(begin, 0.0, Duration());
    end = std::clamp(end, 0.0, Duration());
    if (end <= begin) return {};
    auto left = static_cast<std::size_t>(begin * 48000 / frames_per_peak_) + kMaximumWaveformBins;
    auto right =
            std::min(count_, static_cast<std::size_t>(std::ceil(end * 48000 / frames_per_peak_))) +
            kMaximumWaveformBins;
    WaveformPeak result;
    while (left < right) {
        if (left % 2) result = Merge(result, tree_[left++]);
        if (right % 2) result = Merge(result, tree_[--right]);
        left /= 2;
        right /= 2;
    }
    return result;
}
WaveformPeak ClipWaveformPeak(const WaveformIndex& waveform,
                              const parameters::ClipInterval& interval, double begin, double end) {
    if (!std::isfinite(begin) || !std::isfinite(end)) throw std::invalid_argument("waveform.range");
    const auto& timing = interval.Timing();
    if (timing.end_ == parameters::ClipEnd::kHold)
        throw std::invalid_argument("waveform.audio_hold");
    begin = std::max(begin, timing.start_);
    end = std::min(end, timing.start_ + interval.ActiveDuration());
    if (end <= begin) return {};
    auto offset = (begin - timing.start_) * timing.rate_;
    const auto span = timing.source_out_ - timing.source_in_;
    const auto length = (end - begin) * timing.rate_;
    if (timing.end_ == parameters::ClipEnd::kLoop) {
        if (length >= span) return waveform.Range(timing.source_in_, timing.source_out_);
        offset = std::fmod(offset, span);
        if (offset + length > span)
            return Merge(
                    waveform.Range(timing.source_in_ + offset, timing.source_out_),
                    waveform.Range(timing.source_in_, timing.source_in_ + offset + length - span));
    }
    return waveform.Range(timing.source_in_ + offset, timing.source_in_ + offset + length);
}
}  // namespace rhythm::media
