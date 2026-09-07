#pragma once

// 线性幅度谱 → log 刻度频带能量映射（Monstercat 参数：20Hz–16kHz、63 带）。
// 相邻带边界等比（常数 Q）。低频段 FFT 分辨率不足以区分相邻带，
// 因此对幅度谱做 Catmull-Rom 三次插值，再以 RMS/峰值混合聚合带内采样。
// RMS 保留宽带能量，峰值分量避免小提琴等窄谐波在宽频带内被算术平均稀释。

#include <cstddef>
#include <span>
#include <vector>

namespace rhythm::audio::detail {

class BandMap {
   public:
    BandMap(std::size_t fft_size, double sample_rate, std::size_t num_bands = 63,
            double min_freq = 20.0, double max_freq = 16000.0);

    std::size_t NumBands() const { return num_bands_; }
    double MinFreq() const { return min_freq_; }
    double MaxFreq() const { return max_freq_; }

    // 频带边界（num_bands+1 个，等比分布）
    const std::vector<double>& BandEdges() const { return edges_; }

    // 幅度谱 → 各频带能量（带内插值采样的 RMS/峰值混合）
    void Map(std::span<const float> magnitudes, std::span<float> bands_out) const;

    // 频带编号 → 中心频率
    double BandCenter(std::size_t band) const;

    // 频率 → 频带编号
    int BandForFrequency(double freq) const;

    // 任意频率段能量（bass/mid/treb 用，RMS/峰值混合）
    float SumRange(std::span<const float> magnitudes, double freq_low, double freq_high) const;

   private:
    // 幅度谱在 freq 处的线性插值（桶中心位于 bin*binWidth）
    float InterpolatedMagnitude(std::span<const float> magnitudes, double freq) const;

    std::size_t fft_size_ = 0;
    double sample_rate_ = 0.0;
    std::size_t num_bands_ = 0;
    double min_freq_ = 0.0;
    double max_freq_ = 0.0;
    double bin_width_ = 0.0;
    std::vector<double> edges_{};
};

}  // namespace rhythm::audio::detail
