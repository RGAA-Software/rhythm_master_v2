#include "flow_control.h"

#include <iostream>

namespace rhythm::quic_probe {
void FlowControl(Connection& client, Connection& server, bool cancel_paused) {
    const auto& api = client.api_->Table();
    Check(api.StreamOpen(client.connection_.Get(), QUIC_STREAM_OPEN_FLAG_UNIDIRECTIONAL,
                         Connection::StreamCallback, &client, client.stream_.Output()),
          "flow.open");
    Check(api.StreamStart(client.stream_.Get(), QUIC_STREAM_START_FLAG_IMMEDIATE), "flow.start");
    constexpr std::size_t kMessages = 1000;
    constexpr std::size_t kBatch = 20;
    std::size_t received = 0;
    std::array<std::uint8_t, 4096> payload{};
    for (std::size_t batch = 0; batch < (cancel_paused ? 1 : kMessages / kBatch); ++batch) {
        for (std::size_t index = 0; index < kBatch; ++index) {
            const auto number = batch * kBatch + index;
            payload.fill(static_cast<std::uint8_t>(number));
            payload[0] = static_cast<std::uint8_t>(number >> 8);
            Require(client.Wait([&] {
                client.DrainCompleted();
                return client.send_queue_.Stats().in_flight_count_ < 16;
            }),
                    "flow.send_capacity");
            client.SendBytes(payload, false, number + 1 == kMessages);
        }
        Require(server.Wait([&] { return server.inbox_.Paused(); }), "flow.pause_timeout");
        if (cancel_paused) break;
        while (received < (batch + 1) * kBatch) {
            Require(server.Wait([&] { return server.inbox_.Count() || server.closed_; }),
                    "flow.receive_timeout");
            bool resume = false;
            {
                std::lock_guard lock(server.mutex_);
                Require(!server.closed_, "flow.closed_early");
                while (const auto message = server.inbox_.Take()) {
                    const auto bytes = message->Bytes();
                    Require(bytes.size() == payload.size() &&
                                    bytes[0] == static_cast<std::uint8_t>(received >> 8) &&
                                    std::all_of(bytes.begin() + 1, bytes.end(),
                                                [received](auto value) {
                                                    return value ==
                                                           static_cast<std::uint8_t>(received);
                                                }),
                            "flow.message_order_or_bytes");
                    ++received;
                }
                resume = server.inbox_.TakeResume();
            }
            if (resume)
                Check(api.StreamReceiveSetEnabled(server.stream_.Get(), true), "flow.resume");
        }
    }
    if (!cancel_paused)
        Require(server.Wait([&] { return server.inbox_.Finished(); }), "flow.fin_timeout");
    api.ConnectionShutdown(client.connection_.Get(), QUIC_CONNECTION_SHUTDOWN_FLAG_NONE, 0);
    Require(client.Wait([&] { return client.closed_; }), "flow.shutdown");
    client.DrainSends();
    Require(server.Wait([&] { return server.closed_; }), "flow.server_shutdown");
    server.DrainSends();
    Require(server.inbox_.PeakBytes() <= 65536 && server.inbox_.PeakCount() <= 64 &&
                    server.inbox_.Pauses(),
            "flow.receive_budget");
    std::cout << "QUIC receive flow control: messages=" << received
              << " peak_bytes=" << server.inbox_.PeakBytes() << " pauses=" << server.inbox_.Pauses()
              << " cancelled_paused=" << cancel_paused << '\n';
}
}  // namespace rhythm::quic_probe
