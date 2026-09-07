#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

#include "rhythm/transport/transport.h"

namespace {
using rhythm::cluster::QueueResult;
using rhythm::cluster::SendChannel;
using rhythm::cluster::SendPayload;
using rhythm::transport::ConnectionHandle;
using rhythm::transport::Event;
using rhythm::transport::EventKind;
using rhythm::transport::Transport;
using namespace std::chrono_literals;
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
SendPayload Payload(std::size_t size, std::uint8_t marker) {
    return SendPayload(std::vector<std::uint8_t>(size, marker));
}
void CheckBudget(const std::vector<Event>& events) {
    Require(events.size() <= 128, "event.count_budget");
    std::size_t bytes = 0;
    for (const auto& event : events) bytes += event.payload_.Bytes().size();
    Require(bytes <= 1048576, "event.byte_budget");
}
struct Pair {
    rhythm::security::HostIdentity identity_ = rhythm::security::HostIdentity::Create();
    Transport host_ = Transport::Listen(
            identity_, {rhythm::cluster_auth::AddressFamily::kIpv4, {127, 0, 0, 1}, 0}, 1);
    Transport client_ = Transport::Client();
    ConnectionHandle client_id_{};
    ConnectionHandle host_id_{};
    bool client_connected_ = false;
    bool host_connected_ = false;
    bool client_closed_ = false;
    bool host_closed_ = false;
    bool client_datagrams_ = false;
    bool host_datagrams_ = false;
    rhythm::transport::CloseReason client_reason_ = rhythm::transport::CloseReason::kNone;
    std::vector<Event> host_data_{};
    std::vector<Event> client_data_{};
    void Connect(bool valid = true) {
        auto pin = identity_.Fingerprint();
        if (!valid) pin[0] ^= 1;
        const auto connection = client_.Connect(*host_.LocalEndpoint(), pin);
        Require(connection.has_value(), "connect.reservation");
        client_id_ = *connection;
    }
    void Tick() {
        for (int side = 0; side < 2; ++side) {
            auto events = side ? client_.Poll() : host_.Poll();
            CheckBudget(events);
            for (auto& event : events) {
                switch (event.kind_) {
                    case EventKind::kConnected:
                        if (side)
                            client_connected_ = true;
                        else {
                            host_connected_ = true;
                            host_id_ = event.connection_;
                        }
                        break;
                    case EventKind::kDisconnected:
                        if (side) {
                            client_closed_ = true;
                            client_reason_ = event.reason_;
                        } else
                            host_closed_ = true;
                        break;
                    case EventKind::kCapabilities:
                        Require(event.maximum_datagram_bytes_ <= 1024, "datagram.exposed_budget");
                        (side ? client_datagrams_ : host_datagrams_) =
                                event.maximum_datagram_bytes_ > 0;
                        break;
                    default:
                        Require(side ? client_connected_ : host_connected_, "event.order");
                        (side ? client_data_ : host_data_).push_back(std::move(event));
                }
            }
        }
    }
    template <typename Predicate>
    void Until(Predicate done, const char* message) {
        const auto deadline = std::chrono::steady_clock::now() + 8s;
        do {
            Tick();
            if (done()) return;
            std::this_thread::sleep_for(1ms);
        } while (std::chrono::steady_clock::now() < deadline);
        throw std::runtime_error(message);
    }
    void Ready() {
        Connect();
        Until(
                [&] {
                    return host_connected_ && client_connected_ && host_datagrams_ &&
                           client_datagrams_;
                },
                "connect.timeout");
    }
};
void Channels() {
    Pair pair;
    pair.Ready();
    Require(pair.client_.Send(pair.client_id_, SendChannel::kAsset, Payload(10, 1)) ==
                    QueueResult::kInvalid,
            "asset.direction");
    Require(pair.client_.Send(pair.client_id_, SendChannel::kControl, Payload(4097, 1)) ==
                    QueueResult::kInvalid,
            "control.limit");
    Require(pair.client_.Send(pair.client_id_, SendChannel::kRealtime, Payload(1025, 1)) ==
                    QueueResult::kInvalid,
            "datagram.limit");
    Require(pair.client_.Send(pair.client_id_, SendChannel::kControl, Payload(4096, 42)) ==
                    QueueResult::kAccepted,
            "control.enqueue");
    pair.Until([&] { return pair.host_data_.size() == 1; }, "control.receive");
    Require(pair.host_data_[0].kind_ == EventKind::kControl &&
                    pair.host_data_[0].payload_.Bytes().size() == 4096 &&
                    pair.host_data_[0].payload_.Bytes().front() == 42,
            "control.bytes");
    Require(pair.host_.Send(pair.host_id_, SendChannel::kControl, Payload(31, 7)) ==
                    QueueResult::kAccepted,
            "control.reply");
    Require(pair.host_.Send(pair.host_id_, SendChannel::kAsset, Payload(65536, 13)) ==
                    QueueResult::kAccepted,
            "asset.enqueue");
    Require(pair.host_.Send(pair.host_id_, SendChannel::kRealtime, Payload(100, 17)) ==
                    QueueResult::kAccepted,
            "datagram.enqueue");
    pair.Until([&] { return pair.client_data_.size() == 3; }, "channels.receive");
    for (const auto& event : pair.client_data_) {
        const auto marker = event.kind_ == EventKind::kControl ? 7
                            : event.kind_ == EventKind::kAsset ? 13
                                                               : 17;
        Require(std::all_of(event.payload_.Bytes().begin(), event.payload_.Bytes().end(),
                            [&](auto byte) { return byte == marker; }),
                "channels.bytes");
    }
    pair.client_.Disconnect(pair.client_id_);
    pair.Until([&] { return pair.client_closed_ && pair.host_closed_; }, "close.timeout");
    Require(pair.client_.Send(pair.client_id_, SendChannel::kControl, Payload(1, 0)) ==
                    QueueResult::kClosed,
            "close.stale_handle");
    const auto old_id = pair.client_id_;
    pair.client_connected_ = pair.host_connected_ = pair.client_closed_ = pair.host_closed_ = false;
    pair.Ready();
    Require(pair.client_id_.value_ > old_id.value_, "connection.id_reuse");
    pair.host_.Stop();
    pair.host_.Stop();
    Require(pair.host_.Poll().empty() && !pair.host_.LocalEndpoint(), "stop.idempotent");
}
void CertificateAndCancellation() {
    {
        Pair pair;
        pair.Ready();
        Require(pair.client_.Send(pair.client_id_, SendChannel::kControl, Payload(100, 27)) ==
                        QueueResult::kAccepted,
                "drain.enqueue");
        pair.client_.DrainAndDisconnect(pair.client_id_);
        Require(pair.client_.Send(pair.client_id_, SendChannel::kControl, Payload(1, 0)) ==
                        QueueResult::kClosed,
                "drain.reject_new");
        // Simulate an owner that does not poll until after the native close.
        for (int tick = 0; tick < 150; ++tick) {
            CheckBudget(pair.client_.Poll());
            std::this_thread::sleep_for(1ms);
        }
        pair.Until([&] { return pair.host_data_.size() == 1; }, "drain.delivery");
        Require(pair.host_data_[0].payload_.Bytes().front() == 27, "drain.bytes");
        pair.Until([&] { return pair.host_closed_; }, "drain.close");
    }
    {
        Pair pair;
        pair.Connect(false);
        pair.Until([&] { return pair.client_closed_; }, "certificate.timeout");
        Require(!pair.client_connected_ &&
                        pair.client_reason_ == rhythm::transport::CloseReason::kCertificate,
                "certificate.reject");
    }
    for (int repeat = 0; repeat < 5; ++repeat) {
        Pair pair;
        pair.Connect();
        pair.client_.Disconnect(pair.client_id_);
        pair.Until([&] { return pair.client_closed_; }, "handshake.cancel");
    }
    Pair pair;
    pair.Ready();
    for (int index = 0; index < 16; ++index)
        Require(pair.client_.Send(pair.client_id_, SendChannel::kControl, Payload(4096, 1)) ==
                        QueueResult::kAccepted,
                "cancel.enqueue");
    pair.client_.Poll();
    pair.client_.Stop();
    pair.host_.Stop();
}
void BoundedFlow() {
    Pair pair;
    pair.Ready();
    Require(!pair.client_.Connect(*pair.host_.LocalEndpoint(), pair.identity_.Fingerprint()),
            "client.capacity");
    auto overflow = Transport::Client();
    const auto rejected =
            overflow.Connect(*pair.host_.LocalEndpoint(), pair.identity_.Fingerprint());
    Require(rejected.has_value(), "host.capacity_attempt");
    bool rejected_closed = false;
    pair.Until(
            [&] {
                for (const auto& event : overflow.Poll()) {
                    Require(event.kind_ != EventKind::kConnected, "host.capacity_accept");
                    if (event.kind_ == EventKind::kDisconnected) rejected_closed = true;
                }
                return rejected_closed;
            },
            "host.capacity_timeout");
    // Deliberately do not poll host while sender fills its window.
    std::size_t sent = 0;
    for (int batch = 0; batch < 16; ++batch) {
        for (;;) {
            const auto result =
                    pair.client_.Send(pair.client_id_, SendChannel::kControl,
                                      Payload(4096, static_cast<std::uint8_t>(sent % 251)));
            if (result == QueueResult::kFull) break;
            Require(result == QueueResult::kAccepted, "flow.enqueue");
            ++sent;
        }
        CheckBudget(pair.client_.Poll());
        std::this_thread::sleep_for(3ms);
    }
    Require(sent >= 16 && sent <= 256, "flow.bounded_sender");
    pair.Until([&] { return pair.host_data_.size() == sent; }, "flow.resume");
    for (std::size_t index = 0; index < sent; ++index) {
        const auto bytes = pair.host_data_[index].payload_.Bytes();
        Require(bytes.size() == 4096 && std::all_of(bytes.begin(), bytes.end(),
                                                    [&](auto byte) { return byte == index % 251; }),
                "flow.order_bytes");
    }
    bool wrong_thread_rejected = false;
    std::thread other([&] {
        try {
            pair.client_.Poll();
        } catch (const std::logic_error&) {
            wrong_thread_rejected = true;
        }
    });
    other.join();
    Require(wrong_thread_rejected, "owner.thread");
}
}  // namespace
int TransportLan(int argc, char** argv);
int main(int argc, char** argv) {
    try {
        if (argc > 1) return TransportLan(argc, argv);
        Channels();
        CertificateAndCancellation();
        BoundedFlow();
        std::cout << "Transport public contracts passed: channels, pin, cancel, reconnect, "
                     "capacity, flow, owner thread\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
