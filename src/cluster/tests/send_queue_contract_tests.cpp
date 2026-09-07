#include <iostream>
#include <stdexcept>

#include "rhythm/cluster/send_queue.h"

int main() {
    using namespace rhythm::cluster;
    try {
        const auto check = [](bool value) {
            if (!value) throw std::runtime_error("cluster.send_queue_contract");
        };
        std::vector<std::uint8_t> source(1024, 7);
        const SendPayload shared(source);
        source[0] = 9;
        check(shared.Bytes()[0] == 7);
        SendQueue queue(1);
        const SendPayload control(std::vector<std::uint8_t>(4096, 1));
        const SendPayload asset(std::vector<std::uint8_t>(65536, 2));
        for (int index = 0; index < 4; ++index)
            check(queue.Enqueue(SendChannel::kAsset, asset) == QueueResult::kAccepted);
        check(queue.Enqueue(SendChannel::kAsset, shared) == QueueResult::kFull);
        check(queue.Enqueue(SendChannel::kControl, control) == QueueResult::kAccepted);
        check(queue.Enqueue(SendChannel::kRealtime, shared) == QueueResult::kAccepted);
        const auto first = queue.Acquire().value();
        check(first.channel_ == SendChannel::kControl);
        const auto realtime = queue.Acquire().value();
        check(realtime.channel_ == SendChannel::kRealtime);
        check(queue.Enqueue(SendChannel::kRealtime, shared) == QueueResult::kAccepted);
        check(queue.Enqueue(SendChannel::kRealtime, shared) == QueueResult::kReplaced);
        check(queue.Acquire()->channel_ == SendChannel::kAsset);
        check(queue.Enqueue(SendChannel::kAsset, asset) == QueueResult::kFull);
        check(queue.Stats().pending_bytes_ + queue.Stats().in_flight_bytes_ ==
              262144 + 4096 + 2048);
        check(queue.Complete(realtime.ticket_) && !queue.Complete(realtime.ticket_));
        check(queue.Acquire()->channel_ == SendChannel::kRealtime);
        const auto in_flight = queue.Stats().in_flight_bytes_;
        queue.Close();
        check(!queue.Acquire() && queue.Stats().pending_bytes_ == 0 &&
              queue.Stats().in_flight_bytes_ == in_flight);
        check(queue.Enqueue(SendChannel::kControl, shared) == QueueResult::kClosed);
        bool rejected = false;
        try {
            queue.Reset(2);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected);
        SendQueue bounded(2);
        std::vector<SendTicket> tickets;
        for (int index = 0; index < 16; ++index) {
            check(bounded.Enqueue(SendChannel::kControl, control) == QueueResult::kAccepted);
            tickets.push_back(bounded.Acquire()->ticket_);
        }
        check(!bounded.Acquire());
        check(bounded.Enqueue(SendChannel::kControl, shared) == QueueResult::kFull);
        check(!bounded.Complete({3, tickets.front().id_}));
        check(bounded.Complete(tickets.front()));
        check(bounded.Enqueue(SendChannel::kControl, control) == QueueResult::kAccepted);
        const auto replacement = bounded.Acquire().value();
        check(replacement.ticket_ != tickets.front());
        bounded.Close();
        for (std::size_t index = 1; index < tickets.size(); ++index)
            check(bounded.Complete(tickets[index]));
        check(bounded.Complete(replacement.ticket_));
        check(bounded.Stats().in_flight_bytes_ == 0 && bounded.Stats().in_flight_count_ == 0);
        bounded.Reset(3);
        check(!bounded.Complete(replacement.ticket_));
        rejected = false;
        try {
            bounded.Reset(2);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected);
        const SendPayload tiny(std::array<std::uint8_t, 1>{1});
        for (int index = 0; index < 64; ++index)
            check(bounded.Enqueue(SendChannel::kControl, tiny) == QueueResult::kAccepted);
        check(bounded.Enqueue(SendChannel::kControl, tiny) == QueueResult::kFull);
        check(bounded.Enqueue(SendChannel::kControl, {}) == QueueResult::kInvalid);
        check(bounded.Enqueue(SendChannel::kRealtime, control) == QueueResult::kInvalid);
        // Null peers model application queue saturation, not sockets/AP capacity.
        std::deque<SendQueue> peers;
        for (std::uint64_t index = 1; index <= 1000; ++index) peers.emplace_back(index);
        for (int frame = 0; frame < 120; ++frame) {
            for (auto& peer : peers) {
                const auto result = peer.Enqueue(SendChannel::kRealtime, shared);
                check(result == QueueResult::kAccepted || result == QueueResult::kReplaced);
                if (frame == 0) check(peer.Acquire().has_value());
                check(peer.Stats().pending_bytes_ + peer.Stats().in_flight_bytes_ <= 2048);
                check(peer.Stats().in_flight_count_ == 1);
            }
        }
        for (const auto& peer : peers) check(peer.Stats().replaced_realtime_ == 118);
        std::cout << "send queue contracts passed: byte/item/in-flight budgets, priority, "
                     "immutable sharing, cancellation, 1000 stalled null peers\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
