#include "rhythm/cluster/clock.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace rhythm::cluster {
namespace {
constexpr std::int64_t kMaximumTimestamp = std::int64_t{1} << 52;
constexpr double kSampleAgeUs = 30000000;
constexpr double kDelayWindowUs = 5000;
constexpr double kMaximumDelayUs = 250000;
constexpr double kMaximumStepUs = 250000;
constexpr std::int64_t kStaleUs = 5000000;
bool ValidTime(std::int64_t value) { return value >= 0 && value <= kMaximumTimestamp; }
}  // namespace
ClockSynchronizer::ClockSynchronizer(std::uint64_t epoch) { Reset(epoch); }
void ClockSynchronizer::Reset(std::uint64_t epoch) {
    if (!epoch) throw std::invalid_argument("cluster.epoch");
    const auto watermark = epoch == epoch_ ? sequence_ : 0;
    epoch_ = epoch;
    samples_ = {};
    count_ = 0;
    next_ = 0;
    sequence_ = watermark;
    received_us_ = 0;
    reference_us_ = 0;
    offset_us_ = 0;
    drift_ppm_ = 0;
    uncertainty_us_ = 0;
    ready_ = false;
    resync_required_ = false;
    last_local_us_.reset();
    last_host_us_ = 0;
}
ProbeResult ClockSynchronizer::Observe(ClockProbe probe) {
    if (probe.epoch_ != epoch_) return ProbeResult::kOldEpoch;
    if (!probe.sequence_ || probe.sequence_ <= sequence_) return ProbeResult::kReplay;
    for (const auto time : {probe.local_send_us_, probe.host_receive_us_, probe.host_send_us_,
                            probe.local_receive_us_})
        if (!ValidTime(time)) return ProbeResult::kInvalid;
    if (probe.local_receive_us_ < probe.local_send_us_ ||
        probe.host_send_us_ < probe.host_receive_us_ ||
        (count_ && probe.local_receive_us_ < received_us_))
        return ProbeResult::kInvalid;
    if (resync_required_) return ProbeResult::kResyncRequired;
    // RFC 5905 section 8: offset and round-trip delay from four timestamps.
    // Units are session microseconds, not NTP wall-clock timestamps.
    const auto delay = double((probe.local_receive_us_ - probe.local_send_us_) -
                              (probe.host_send_us_ - probe.host_receive_us_));
    const auto offset = 0.5 * (double(probe.host_receive_us_ - probe.local_send_us_) +
                               double(probe.host_send_us_ - probe.local_receive_us_));
    const auto middle = 0.5 * (double(probe.local_send_us_) + double(probe.local_receive_us_));
    if (delay < -200 || delay > kMaximumDelayUs ||
        probe.host_send_us_ - probe.host_receive_us_ > 100000)
        return ProbeResult::kDelayed;
    double minimum_delay = kMaximumDelayUs;
    for (std::size_t index = 0; index < count_; ++index)
        if (middle - samples_[index].local_us_ <= kSampleAgeUs)
            minimum_delay = std::min(minimum_delay, samples_[index].delay_us_);
    if (count_ >= 3 && delay > minimum_delay + kDelayWindowUs) return ProbeResult::kDelayed;
    const auto predicted = offset_us_ + drift_ppm_ * (middle - reference_us_) / 1000000;
    if (ready_ && std::abs(offset - predicted) > kMaximumStepUs) {
        resync_required_ = true;
        return ProbeResult::kResyncRequired;
    }
    samples_[next_] = {middle, offset, std::max(0.0, delay)};
    next_ = (next_ + 1) % samples_.size();
    count_ = std::min(count_ + 1, samples_.size());
    sequence_ = probe.sequence_;
    received_us_ = probe.local_receive_us_;
    Fit(middle);
    return ProbeResult::kAccepted;
}
void ClockSynchronizer::Fit(double now_us) {
    double minimum_delay = kMaximumDelayUs;
    for (std::size_t index = 0; index < count_; ++index)
        if (now_us - samples_[index].local_us_ <= kSampleAgeUs)
            minimum_delay = std::min(minimum_delay, samples_[index].delay_us_);
    double sum_x = 0;
    double sum_y = 0;
    double sum_xx = 0;
    double sum_xy = 0;
    std::size_t selected = 0;
    const auto included = [&](const Sample& sample) {
        return now_us - sample.local_us_ <= kSampleAgeUs &&
               sample.delay_us_ <= minimum_delay + kDelayWindowUs;
    };
    for (std::size_t index = 0; index < count_; ++index) {
        const auto& sample = samples_[index];
        if (!included(sample)) continue;
        const auto x = (sample.local_us_ - now_us) / 1000000;
        sum_x += x;
        sum_y += sample.offset_us_;
        sum_xx += x * x;
        sum_xy += x * sample.offset_us_;
        ++selected;
    }
    const auto n = static_cast<double>(selected);
    const auto variance = n * sum_xx - sum_x * sum_x;
    drift_ppm_ =
            variance > 1 ? std::clamp((n * sum_xy - sum_x * sum_y) / variance, -1000.0, 1000.0) : 0;
    offset_us_ = (sum_y - drift_ppm_ * sum_x) / n;
    reference_us_ = now_us;
    double residual = 0;
    for (std::size_t index = 0; index < count_; ++index) {
        const auto& sample = samples_[index];
        if (included(sample))
            residual = std::max(residual,
                                std::abs(sample.offset_us_ - offset_us_ -
                                         drift_ppm_ * (sample.local_us_ - now_us) / 1000000));
    }
    uncertainty_us_ = minimum_delay / 2 + residual;
    ready_ = selected >= 3 && variance > 1;
}
ClockEstimate ClockSynchronizer::Estimate(std::int64_t local_us) const {
    ClockEstimate result;
    result.resync_required_ = resync_required_;
    if (!ValidTime(local_us) || !ready_ || resync_required_ || local_us < received_us_ ||
        local_us - received_us_ > kStaleUs)
        return result;
    result.host_us_ = double(local_us) + offset_us_ +
                      drift_ppm_ * (double(local_us) - reference_us_) / 1000000;
    if (result.host_us_ < 0 || result.host_us_ > kMaximumTimestamp) return result;
    result.ready_ = true;
    result.drift_ppm_ = drift_ppm_;
    result.uncertainty_us_ = uncertainty_us_ + (local_us - received_us_) * 0.001;
    return result;
}
std::optional<std::int64_t> ClockSynchronizer::Advance(std::int64_t local_us) {
    if (last_local_us_ && (!ValidTime(local_us) || local_us < *last_local_us_ ||
                           local_us - *last_local_us_ > kStaleUs)) {
        resync_required_ = true;
        return std::nullopt;
    }
    const auto estimate = Estimate(local_us);
    if (!estimate.ready_) return std::nullopt;
    if (last_local_us_) {
        const auto elapsed = local_us - *last_local_us_;
        if (elapsed < 0 || elapsed > kStaleUs) {
            resync_required_ = true;
            return std::nullopt;
        }
        const auto predicted = last_host_us_ + elapsed * (1 + drift_ppm_ / 1000000);
        const auto correction =
                std::clamp(estimate.host_us_ - predicted, -elapsed * 0.005, elapsed * 0.005);
        last_host_us_ = std::max(last_host_us_, predicted + correction);
    } else
        last_host_us_ = estimate.host_us_;
    last_local_us_ = local_us;
    return static_cast<std::int64_t>(std::llround(last_host_us_));
}
}  // namespace rhythm::cluster
