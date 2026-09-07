#pragma once

// BPM 估计：onset 间隔（IOI）直方图投票 + 倍频折叠抑制。
// 每个 IOI 先折叠进 [min_bpm_, max_bpm_] 区间再投票（240 会折回 120），
// 胜出 bin 内的 IOI 取均值反推精确 BPM。onset 不足或分布散乱时置信度为 0。

#include <cstddef>
#include <deque>
#include <span>
#include <vector>

namespace rhythm::audio::detail {

class Tempo {
   public:
    struct Config {
        double min_bpm_ = 60.0;
        double max_bpm_ = 200.0;
        std::size_t max_onsets_ = 128;  // 参与估计的最近 onset 数
        std::size_t min_onsets_ = 5;    // 少于此数量置信度为 0
        double min_confidence_ = 0.5;   // 低于此置信度不输出 BPM
    };

    Tempo();
    explicit Tempo(Config config);

    void Reset();
    void AddOnset(double time_seconds, float strength = 1.0f);

    double Bpm() const { return bpm_; }                // 无有效估计时为 0
    double Confidence() const { return confidence_; }  // 0..1

   private:
    void Reestimate();

    Config config_{};
    std::deque<double> onset_times_{};
    double bpm_ = 0.0;
    double confidence_ = 0.0;
};

}  // namespace rhythm::audio::detail
