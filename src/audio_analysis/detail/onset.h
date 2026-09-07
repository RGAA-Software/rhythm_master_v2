#pragma once

// Onset 检测：谱通量（相邻帧频谱正增量之和）+ 长期滑动均值自适应阈值峰值拾取。
// projectM/MilkDrop 均无 onset 事件检测，此为自研（思路参考 aubio/essentia）。
// 检出延迟固定 1 帧（需后一帧确认局部峰值）。

#include <cstddef>
#include <deque>
#include <span>
#include <vector>

namespace rhythm::audio::detail {

struct OnsetEvent {
    std::size_t frame_ = 0;      // 峰值所在帧
    double time_seconds_ = 0.0;  // 峰值时刻（秒）
    float strength_ = 0.0f;      // 峰值通量 / 自适应阈值（>=1）
};

class Onset {
   public:
    struct Config {
        float threshold_ratio_ = 1.5f;     // 阈值 = 长期均值 * ratio
        double refractory_seconds_ = 0.1;  // 两次 onset 最小间隔
        std::size_t mean_window_ = 45;     // 长期均值窗口（帧数，~1.5s@30fps）
        double warmup_seconds_ = 0.3;  // 启动预热时长（秒），之前不检出（帧率无关）
    };

    Onset();
    explicit Onset(Config config);

    void Reset();

    // 每帧喂一份幅度谱；frame 为帧序号，frame_time 为当前帧时刻（秒），dt 为帧间隔（秒）
    void Process(std::span<const float> magnitudes, std::size_t num_bins, std::size_t frame,
                 double frame_time, double dt);

    // Process 后查询：本帧是否确认了新 onset（事件落在上一帧）
    bool HasOnset() const { return has_onset_; }
    const OnsetEvent& LastOnset() const { return last_onset_; }

    float CurrentFlux() const { return flux_; }
    float CurrentThreshold() const { return threshold_; }

   private:
    Config config_{};
    std::vector<float> prev_magnitudes_{};
    std::deque<float> flux_history_{};  // 最近 mean_window_ 帧的通量
    std::size_t last_onset_frame_ = 0;
    bool has_last_onset_frame_ = false;

    float flux_ = 0.0f;
    float threshold_ = 0.0f;
    bool has_onset_ = false;
    OnsetEvent last_onset_{};
};

}  // namespace rhythm::audio::detail
