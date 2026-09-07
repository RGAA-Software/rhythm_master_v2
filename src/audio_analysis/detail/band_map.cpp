#include "band_map.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace rhythm::audio::detail {

namespace {

float AggregateSamples(float square_sum, float peak, int sample_count) {
    if (sample_count <= 0) {
        return 0.0f;
    }
    const float rms = std::sqrt(square_sum / static_cast<float>(sample_count));
    constexpr float kPeakContribution = 0.35f;
    return rms * (1.0f - kPeakContribution) + peak * kPeakContribution;
}

}  // namespace

BandMap::BandMap(std::size_t fft_size, double sample_rate, std::size_t num_bands, double min_freq,
                 double max_freq)
    : fft_size_(fft_size),
      sample_rate_(sample_rate),
      num_bands_(num_bands),
      min_freq_(min_freq),
      max_freq_(max_freq),
      bin_width_(sample_rate / static_cast<double>(fft_size)) {
    if (num_bands == 0 || min_freq <= 0.0 || max_freq <= min_freq) {
        throw std::invalid_argument("BandMap: invalid band range");
    }

    // 等比边界：edge[i] = min_freq * ratio^i
    double const ratio = std::pow(max_freq / min_freq, 1.0 / static_cast<double>(num_bands));
    edges_.resize(num_bands + 1);
    for (std::size_t i = 0; i <= num_bands; i++) {
        edges_[i] = min_freq * std::pow(ratio, static_cast<double>(i));
    }
}

int BandMap::BandForFrequency(double freq) const {
    if (freq <= min_freq_) {
        return 0;
    }
    if (freq >= max_freq_) {
        return static_cast<int>(num_bands_) - 1;
    }
    double const ratio = edges_[1] / edges_[0];
    int band = static_cast<int>(std::floor(std::log(freq / min_freq_) / std::log(ratio)));
    band = std::max(0, std::min(band, static_cast<int>(num_bands_) - 1));
    return band;
}

double BandMap::BandCenter(std::size_t band) const {
    return std::sqrt(edges_[band] * edges_[band + 1]);
}

float BandMap::InterpolatedMagnitude(std::span<const float> magnitudes, double freq) const {
    if (freq > sample_rate_ * 0.5) return 0;
    double const pos = freq / bin_width_;
    std::size_t const num_bins = fft_size_ / 2;
    if (pos <= 0.0) {
        return magnitudes[0];
    }
    if (pos >= static_cast<double>(num_bins - 1)) {
        return magnitudes[num_bins - 1];
    }
    std::size_t const bin = static_cast<std::size_t>(pos);
    float const t = static_cast<float>(pos - static_cast<double>(bin));

    // Catmull-Rom 三次插值：比线性更能还原加窗音调的峰值曲率，
    // 避免低频段多个细分带共享同一对桶时出现能量并列（死带）
    float const m0 = magnitudes[bin > 0 ? bin - 1 : 0];
    float const m1 = magnitudes[bin];
    float const m2 = magnitudes[bin + 1];
    float const m3 = magnitudes[bin + 2 < num_bins ? bin + 2 : num_bins - 1];

    float const value = 0.5f * ((2.0f * m1) + (-m0 + m2) * t +
                                (2.0f * m0 - 5.0f * m1 + 4.0f * m2 - m3) * t * t +
                                (-m0 + 3.0f * m1 - 3.0f * m2 + m3) * t * t * t);
    return value < 0.0f ? 0.0f : value;  // 三次插值可能下冲为负，截断
}

void BandMap::Map(std::span<const float> magnitudes, std::span<float> bands_out) const {
    for (std::size_t band = 0; band < num_bands_; band++) {
        double const low = edges_[band];
        double const high = edges_[band + 1];
        // 采样点数随带内桶数自适应，至少 8 个（低频细分带也能拉开差距）
        int const samples =
                std::max(8, std::min(64, static_cast<int>((high - low) / bin_width_) * 4));

        float square_sum = 0.0f;
        float peak = 0.0f;
        for (int s = 0; s < samples; s++) {
            double const freq = low + (high - low) * (static_cast<double>(s) + 0.5) /
                                              static_cast<double>(samples);
            const float magnitude = InterpolatedMagnitude(magnitudes, freq);
            square_sum += magnitude * magnitude;
            peak = std::max(peak, magnitude);
        }
        bands_out[band] = AggregateSamples(square_sum, peak, samples);
    }
}

float BandMap::SumRange(std::span<const float> magnitudes, double freq_low,
                        double freq_high) const {
    if (freq_high <= freq_low) {
        return 0.0f;
    }
    int const samples =
            std::max(8, std::min(256, static_cast<int>((freq_high - freq_low) / bin_width_) * 4));

    float square_sum = 0.0f;
    float peak = 0.0f;
    for (int s = 0; s < samples; s++) {
        double const freq = freq_low + (freq_high - freq_low) * (static_cast<double>(s) + 0.5) /
                                               static_cast<double>(samples);
        const float magnitude = InterpolatedMagnitude(magnitudes, freq);
        square_sum += magnitude * magnitude;
        peak = std::max(peak, magnitude);
    }
    return AggregateSamples(square_sum, peak, samples);
}

}  // namespace rhythm::audio::detail
