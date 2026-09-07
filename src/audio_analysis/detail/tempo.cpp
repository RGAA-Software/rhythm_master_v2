#include "tempo.h"

#include <algorithm>
#include <cmath>

namespace rhythm::audio::detail {

Tempo::Tempo() : Tempo(Config{}) {}

Tempo::Tempo(Config config) : config_(config) {}

void Tempo::Reset() {
    onset_times_.clear();
    bpm_ = 0.0;
    confidence_ = 0.0;
}

void Tempo::AddOnset(double time_seconds, float /*strength*/) {
    onset_times_.push_back(time_seconds);
    while (onset_times_.size() > config_.max_onsets_) {
        onset_times_.pop_front();
    }
    Reestimate();
}

void Tempo::Reestimate() {
    bpm_ = 0.0;
    confidence_ = 0.0;

    if (onset_times_.size() < config_.min_onsets_) {
        return;
    }

    // 收集连续 onset 间隔，折叠进 [min_bpm_, max_bpm_]
    // 折叠即倍频抑制：IOI=0.25s(240BPM) → ×2 → 120BPM，与真实基频汇合
    std::vector<double> iois;
    for (std::size_t i = 1; i < onset_times_.size(); i++) {
        double const ioi = onset_times_[i] - onset_times_[i - 1];
        if (ioi <= 0.0) {
            continue;
        }
        double bpm = 60.0 / ioi;
        while (bpm < config_.min_bpm_) {
            bpm *= 2.0;
        }
        while (bpm > config_.max_bpm_) {
            bpm /= 2.0;
        }
        iois.push_back(60.0 / bpm);  // 折叠后的等效间隔
    }

    if (iois.size() < config_.min_onsets_ - 1) {
        return;
    }

    // 直方图投票，bin 宽 1 BPM
    int const bin_count = static_cast<int>(config_.max_bpm_ - config_.min_bpm_) + 1;
    std::vector<int> histogram(bin_count, 0);
    std::vector<std::vector<double>> bin_iois(bin_count);
    for (double ioi : iois) {
        double const bpm = 60.0 / ioi;
        int bin = static_cast<int>(std::lround(bpm - config_.min_bpm_));
        bin = std::max(0, std::min(bin, bin_count - 1));
        histogram[bin]++;
        bin_iois[bin].push_back(ioi);
    }

    int best_bin = 0;
    for (int bin = 1; bin < bin_count; bin++) {
        if (histogram[bin] > histogram[best_bin]) {
            best_bin = bin;
        }
    }

    double const confidence =
            static_cast<double>(histogram[best_bin]) / static_cast<double>(iois.size());
    if (confidence < config_.min_confidence_) {
        return;  // 分布散乱，置信度保持 0，不输出虚假 BPM
    }

    // 胜出 bin 内 IOI 均值 → 精确 BPM（消除帧量化误差）
    double sum = 0.0;
    for (double ioi : bin_iois[best_bin]) {
        sum += ioi;
    }
    double const mean_ioi = sum / static_cast<double>(bin_iois[best_bin].size());

    bpm_ = 60.0 / mean_ioi;
    confidence_ = confidence;
}

}  // namespace rhythm::audio::detail
