#include "rhythm/cluster/input_buffer.h"

#include <algorithm>
#include <stdexcept>

namespace rhythm::cluster {
namespace {
constexpr std::int64_t kMaximumTimestamp = std::int64_t{1} << 52;
bool ValidTime(std::int64_t value) { return value >= 0 && value <= kMaximumTimestamp; }
}  // namespace
InputBuffer::InputBuffer(std::uint64_t epoch, std::int64_t hold_us, std::int64_t fade_us)
    : hold_us_(hold_us), fade_us_(fade_us) {
    if (hold_us < 0 || hold_us > 1000000 || fade_us < 1 || fade_us > 1000000)
        throw std::invalid_argument("cluster.input_timing");
    Reset(epoch);
}
void InputBuffer::Reset(std::uint64_t epoch) {
    if (!epoch) throw std::invalid_argument("cluster.epoch");
    if (epoch != epoch_) sequence_ = 0;
    epoch_ = epoch;
    first_ = 0;
    count_ = 0;
    overwritten_ = 0;
    frames_ = {};
}
InputResult InputBuffer::Push(InputFrame frame, std::int64_t host_now_us) {
    if (frame.epoch_ != epoch_) return InputResult::kOldEpoch;
    if (!frame.sequence_ || frame.sequence_ <= sequence_) return InputResult::kReplay;
    if (!ValidTime(host_now_us) || !ValidTime(frame.present_us_) ||
        !runtime::ValidInputs(frame.inputs_))
        return InputResult::kInvalid;
    if (host_now_us - frame.present_us_ > 1000000) return InputResult::kLate;
    if (frame.present_us_ - host_now_us > 500000) return InputResult::kFuture;
    if (count_ && frame.present_us_ <= frames_[(first_ + count_ - 1) % frames_.size()].present_us_)
        return InputResult::kOutOfOrder;
    sequence_ = frame.sequence_;
    if (count_ == frames_.size()) {
        first_ = (first_ + 1) % frames_.size();
        --count_;
        ++overwritten_;
    }
    frames_[(first_ + count_) % frames_.size()] = std::move(frame);
    ++count_;
    return InputResult::kAccepted;
}
std::optional<InputSample> InputBuffer::Sample(std::int64_t present_us) const {
    if (!ValidTime(present_us)) throw std::invalid_argument("cluster.input_time");
    if (!count_ || present_us < frames_[first_].present_us_) return std::nullopt;
    std::size_t lower = 0;
    for (std::size_t index = 1; index < count_; ++index) {
        if (frames_[(first_ + index) % frames_.size()].present_us_ > present_us) break;
        lower = index;
    }
    const auto& first = frames_[(first_ + lower) % frames_.size()];
    InputSample result;
    result.inputs_ = first.inputs_;
    result.age_us_ = present_us - first.present_us_;
    if (lower + 1 < count_) {
        const auto& next = frames_[(first_ + lower + 1) % frames_.size()];
        if (next.present_us_ - first.present_us_ <= 250000) {
            const auto amount =
                    double(result.age_us_) / double(next.present_us_ - first.present_us_);
            for (std::size_t index = 0; index < result.inputs_.controls_.size(); ++index)
                result.inputs_.controls_[index] +=
                        (next.inputs_.controls_[index] - first.inputs_.controls_[index]) * amount;
            return result;
        }
    }
    if (result.age_us_ > hold_us_) {
        result.fresh_ = false;
        result.gain_ =
                1 - std::clamp(double(result.age_us_ - hold_us_) / double(fade_us_), 0.0, 1.0);
        for (auto& value : result.inputs_.controls_) value *= result.gain_;
    }
    return result;
}
}  // namespace rhythm::cluster
