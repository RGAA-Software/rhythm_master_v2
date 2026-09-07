#include "onset.h"

#include <algorithm>
#include <numeric>

namespace rhythm::audio::detail {

Onset::Onset() : Onset(Config{}) {}

Onset::Onset(Config config) : config_(config) {}

void Onset::Reset() {
    prev_magnitudes_.clear();
    flux_history_.clear();
    last_onset_frame_ = 0;
    has_last_onset_frame_ = false;
    flux_ = 0.0f;
    threshold_ = 0.0f;
    has_onset_ = false;
    last_onset_ = OnsetEvent{};
}

void Onset::Process(std::span<const float> magnitudes, std::size_t num_bins, std::size_t frame,
                    double frame_time, double dt) {
    has_onset_ = false;

    // 谱通量：相邻帧正增量之和（负增量不计，衰减不产生 onset）
    float flux = 0.0f;
    if (prev_magnitudes_.size() == num_bins) {
        for (std::size_t bin = 0; bin < num_bins; bin++) {
            flux += std::max(0.0f, magnitudes[bin] - prev_magnitudes_[bin]);
        }
    }
    prev_magnitudes_.assign(magnitudes.begin(), magnitudes.end());

    // 长期均值自适应阈值（窗口只含历史帧，用"过去"衡量"现在"）
    float mean = 0.0f;
    if (!flux_history_.empty()) {
        mean = std::accumulate(flux_history_.begin(), flux_history_.end(), 0.0f) /
               static_cast<float>(flux_history_.size());
    }
    threshold_ = mean * config_.threshold_ratio_;

    // 峰值拾取：确认上一帧（frame-1）是否为局部峰值且超阈值
    if (frame_time - dt >= config_.warmup_seconds_ && flux_history_.size() >= 2) {
        float const candidate = flux_history_.back();  // Previous frame, confirmed by current flux.
        float const before = flux_history_[flux_history_.size() - 2];

        bool const is_local_peak = candidate > before && candidate >= flux;
        bool const above_threshold = threshold_ > 0.0f && candidate > threshold_;

        bool refractory_ok = true;
        if (has_last_onset_frame_ && dt > 0.0) {
            std::size_t const refractory_frames =
                    static_cast<std::size_t>(config_.refractory_seconds_ / dt);
            refractory_ok = (frame - 1 - last_onset_frame_) >= refractory_frames;
        }

        if (is_local_peak && above_threshold && refractory_ok) {
            has_onset_ = true;
            last_onset_.frame_ = frame - 1;
            last_onset_.time_seconds_ = frame_time - dt;  // 峰值帧时刻
            last_onset_.strength_ = candidate / threshold_;
            last_onset_frame_ = frame - 1;
            has_last_onset_frame_ = true;
        }
    }

    // 记录历史（当前帧通量入列）
    flux_history_.push_back(flux);
    while (flux_history_.size() > config_.mean_window_) {
        flux_history_.pop_front();
    }

    flux_ = flux;
}

}  // namespace rhythm::audio::detail
