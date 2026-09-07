#include <iostream>
#include <limits>
#include <stdexcept>

#include "rhythm/cluster/input_buffer.h"

int main() {
    using namespace rhythm::cluster;
    try {
        const auto check = [](bool value) {
            if (!value) throw std::runtime_error("input.contract");
        };
        InputBuffer buffer(7);
        check(!buffer.Sample(0));
        InputFrame first{7, 1, 1000000};
        first.inputs_.index_ = 1;
        check(buffer.Push(first, 1000000) == InputResult::kAccepted);
        auto second = first;
        second.sequence_ = 2;
        second.present_us_ = 1100000;
        second.inputs_.index_ = 2;
        second.inputs_.controls_[0] = 1;
        check(buffer.Push(second, 1000000) == InputResult::kAccepted);
        check(!buffer.Sample(999999));
        auto sample = buffer.Sample(1050000);
        check(sample && sample->inputs_.controls_[0] == 0.5 && sample->inputs_.index_ == 1);
        check(buffer.Sample(1100000)->inputs_.index_ == 2);
        check(buffer.Sample(1200000)->inputs_.controls_[0] == 1);
        sample = buffer.Sample(1300000);
        check(sample && !sample->fresh_ && sample->gain_ == 0.5 &&
              sample->inputs_.controls_[0] == 0.5);
        check(buffer.Sample(1400000)->inputs_.controls_[0] == 0);
        InputBuffer gap(7);
        gap.Push(first, 1000000);
        auto distant = second;
        distant.present_us_ = 2000000;
        gap.Push(distant, 1500000);
        check(gap.Sample(1500000)->gain_ == 0 && gap.Sample(2000000)->inputs_.controls_[0] == 1);
        check(buffer.Push(second, 1100000) == InputResult::kReplay);
        second.sequence_ = 3;
        check(buffer.Push(second, 1100000) == InputResult::kOutOfOrder);
        second.present_us_ = 1600000;
        check(buffer.Push(second, 1000000) == InputResult::kFuture);
        check(buffer.Push(second, 2700000) == InputResult::kLate);
        second.inputs_.controls_[0] = std::numeric_limits<double>::quiet_NaN();
        check(buffer.Push(second, 1600000) == InputResult::kInvalid);
        buffer.Reset(7);
        check(!buffer.Sample(1100000) && buffer.Push(first, 1000000) == InputResult::kReplay);
        buffer.Reset(8);
        check(buffer.Push(first, 1000000) == InputResult::kOldEpoch);
        for (std::uint64_t index = 1; index <= 10000; ++index) {
            InputFrame frame{8, index, static_cast<std::int64_t>(index * 16000)};
            frame.inputs_.controls_[0] = 0.75;
            check(buffer.Push(frame, frame.present_us_) == InputResult::kAccepted);
        }
        check(buffer.Size() == 32 && buffer.Overwritten() == 9968 &&
              buffer.Sample(160000000)->inputs_.controls_[0] == 0.75);
        std::cout << "input contracts passed: interpolation, discrete roles, expiry, replay, fixed "
                     "capacity\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
