#include <iostream>
#include <stdexcept>

#include "rhythm/cluster/clock_exchange.h"

int main() {
    using namespace rhythm::cluster;
    try {
        const auto check = [](bool value) {
            if (!value) throw std::runtime_error("clock.exchange_contract");
        };
        ClockExchange exchange(7);
        ClockSynchronizer clock(7);
        const auto request = exchange.Begin(1000).value();
        ClockReply reply{7, request.sequence_, 1000, 11100, 11200};
        auto wrong = reply;
        wrong.epoch_ = 8;
        check(!exchange.Complete(wrong, 1300));
        wrong = reply;
        ++wrong.sequence_;
        check(!exchange.Complete(wrong, 1300));
        wrong = reply;
        ++wrong.local_send_us_;
        check(!exchange.Complete(wrong, 1300));
        wrong = reply;
        wrong.host_send_us_ = 15000;
        check(!exchange.Complete(wrong, 1300));
        const auto probe = exchange.Complete(reply, 1300).value();
        check(probe.local_receive_us_ == 1300 && probe.local_send_us_ == 1000);
        check(clock.Observe(probe) == ProbeResult::kAccepted);
        check(!exchange.Complete(reply, 1300));
        check(!exchange.Begin(1299) && !exchange.Begin(-1));
        check(!exchange.Begin((std::int64_t{1} << 52) + 1));
        std::array<ClockRequest, 8> requests{};
        for (auto& value : requests) value = exchange.Begin(2000).value();
        check(!exchange.Begin(2000));
        // Out-of-order matching is valid; the estimator applies its own monotonic
        // sequence acceptance. A matched reply cannot consume another request.
        const auto last = requests.back();
        check(exchange.Complete({7, last.sequence_, 2000, 12010, 12020}, 2030).has_value());
        const auto first = requests.front();
        check(exchange.Complete({7, first.sequence_, 2000, 12010, 12020}, 2030).has_value());
        check(exchange.Begin(2030).has_value());
        check(!exchange.Complete({7, requests[1].sequence_, 2000, 12010, 12020}, 1002001));
        const auto expired_replacement = exchange.Begin(1002001).value();
        check(expired_replacement.sequence_ > last.sequence_);
        exchange.Reset(7);
        const auto after_reset = exchange.Begin(1002001).value();
        check(after_reset.sequence_ > expired_replacement.sequence_);
        check(!exchange.Complete({7, expired_replacement.sequence_, 1002001, 1002010, 1002020},
                                 1002030));
        exchange.Reset(8);
        check(exchange.Begin(0)->sequence_ == 1);
        check(!exchange.Complete({7, 1, 0, 1, 2}, 3));
        check(exchange.Complete({8, 1, 0, 1, 2}, 3).has_value());
        // A long session of lost probes never increases the fixed pending budget.
        for (std::int64_t tick = 1; tick <= 10000; ++tick)
            check(exchange.Begin(tick * 1000001).has_value());
        std::cout << "clock exchange contracts passed: matching, local timestamp, duplicate, "
                     "timeout, reset, capacity\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
