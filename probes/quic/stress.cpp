#include "stress.h"

#include <chrono>
#include <iostream>

#include "connection.h"
#include "metrics.h"

namespace rhythm::quic_probe {
namespace {
class BoundedListener final {
   public:
    BoundedListener(std::shared_ptr<Api> api, const Handle& configuration, std::size_t count)
        : api_(std::move(api)), configuration_(configuration.Get()) {
        peers_.reserve(count);
        for (std::size_t index = 0; index < count; ++index)
            peers_.push_back(std::make_shared<Connection>(api_));
    }
    static QUIC_STATUS QUIC_API Callback(HQUIC, void* context, QUIC_LISTENER_EVENT* event) {
        if (!context || !event) return QUIC_STATUS_INVALID_PARAMETER;
        if (event->Type != QUIC_LISTENER_EVENT_NEW_CONNECTION) return QUIC_STATUS_SUCCESS;
        auto& self = *static_cast<BoundedListener*>(context);
        std::lock_guard lock(self.mutex_);
        if (self.accepted_ == self.peers_.size()) {
            ++self.rejected_;
            return QUIC_STATUS_CONNECTION_REFUSED;
        }
        auto& peer = *self.peers_[self.accepted_++];
        std::lock_guard peer_lock(peer.mutex_);
        const auto connection = event->NEW_CONNECTION.Connection;
        self.api_->Table().SetCallbackHandler(connection,
                                              reinterpret_cast<void*>(Connection::Callback), &peer);
        const auto status =
                self.api_->Table().ConnectionSetConfiguration(connection, self.configuration_);
        if (QUIC_FAILED(status)) return status;  // Native library owns rejected connection.
        peer.connection_.Adopt(connection);
        return QUIC_STATUS_SUCCESS;
    }
    std::shared_ptr<Api> api_{};
    // Borrowed native configuration outlives the complete stress scope.
    HQUIC configuration_ = nullptr;
    std::mutex mutex_{};
    std::vector<std::shared_ptr<Connection>> peers_{};
    std::size_t accepted_ = 0;
    std::size_t rejected_ = 0;
    // Declared last: listener close quiesces accepts before releasing peer state.
    Handle handle_{api_, Kind::kListener};
};
}  // namespace
void StressConnections(std::shared_ptr<Api> api, const Handle& registration,
                       const Handle& server_config, const Handle& client_config,
                       std::span<const std::uint8_t> certificate, const QUIC_BUFFER& alpn,
                       std::size_t count) {
    Require(count > 0 && count <= 1000, "stress.peer_budget");
    const auto started = std::chrono::steady_clock::now();
    const auto before = Measure();
    BoundedListener listener(api, server_config, count);
    QUIC_ADDR address{};
    Require(QuicAddrFromString("127.0.0.1", 0, &address), "stress.endpoint");
    Check(api->Table().ListenerOpen(registration.Get(), BoundedListener::Callback, &listener,
                                    listener.handle_.Output()),
          "stress.listener");
    Check(api->Table().ListenerStart(listener.handle_.Get(), &alpn, 1, &address), "stress.listen");
    std::uint32_t length = sizeof(address);
    Check(api->Table().GetParam(listener.handle_.Get(), QUIC_PARAM_LISTENER_LOCAL_ADDRESS, &length,
                                &address),
          "stress.port");
    const auto open = [&] {
        auto peer = std::make_shared<Connection>(api);
        peer->expected_certificate_.assign(certificate.begin(), certificate.end());
        Check(api->Table().ConnectionOpen(registration.Get(), Connection::Callback, peer.get(),
                                          peer->connection_.Output()),
              "stress.open");
        Check(api->Table().ConnectionStart(peer->connection_.Get(), client_config.Get(),
                                           QUIC_ADDRESS_FAMILY_INET, "127.0.0.1",
                                           QuicAddrGetPort(&address)),
              "stress.connect");
        return peer;
    };
    std::vector<std::shared_ptr<Connection>> clients;
    clients.reserve(count);
    for (std::size_t index = 0; index < count; ++index) clients.push_back(open());
    for (const auto& peer : clients) {
        Require(peer->Wait([&] {
            return peer->closed_ ||
                   (peer->connected_ && peer->certificate_accepted_ && peer->datagram_enabled_);
        }),
                "stress.handshake_timeout");
        std::lock_guard lock(peer->mutex_);
        Require(peer->connected_ && peer->certificate_accepted_ && peer->datagram_enabled_ &&
                        !peer->closed_,
                "stress.handshake_failed");
    }
    const auto connected = std::chrono::steady_clock::now();
    std::cout << "Established " << count << " concurrently live pinned-TLS connections"
              << std::endl;
    auto excess = open();
    Require(excess->Wait([&] { return excess->closed_; }), "stress.limit_rejection_timeout");
    {
        std::lock_guard lock(excess->mutex_);
        Require(!excess->connected_, "stress.limit_not_enforced");
    }
    excess->DrainSends();
    {
        std::lock_guard lock(listener.mutex_);
        Require(listener.accepted_ == count && listener.rejected_ > 0, "stress.admission_counters");
    }
    for (const auto& peer : clients) {
        peer->Send(true);
        Check(api->Table().StreamOpen(peer->connection_.Get(), QUIC_STREAM_OPEN_FLAG_UNIDIRECTIONAL,
                                      Connection::StreamCallback, peer.get(),
                                      peer->stream_.Output()),
              "stress.stream_open");
        Check(api->Table().StreamStart(peer->stream_.Get(), QUIC_STREAM_START_FLAG_IMMEDIATE),
              "stress.stream_start");
        peer->Send(false);
    }
    for (const auto& peer : listener.peers_) {
        Require(peer->Wait([&] {
            return peer->stream_bytes_.size() == peer->payload_.size() &&
                   peer->datagram_bytes_.size() == peer->payload_.size();
        }),
                "stress.receive_timeout");
        std::lock_guard lock(peer->mutex_);
        Require(std::equal(peer->stream_bytes_.begin(), peer->stream_bytes_.end(),
                           peer->payload_.begin()) &&
                        std::equal(peer->datagram_bytes_.begin(), peer->datagram_bytes_.end(),
                                   peer->payload_.begin()),
                "stress.payload_bytes");
    }
    // Issue all shutdowns before waiting; no project thread is created per peer.
    for (const auto& peer : clients)
        api->Table().ConnectionShutdown(peer->connection_.Get(), QUIC_CONNECTION_SHUTDOWN_FLAG_NONE,
                                        0);
    for (const auto& peer : clients) {
        Require(peer->Wait([&] { return peer->closed_; }), "stress.shutdown_timeout");
        peer->DrainSends();
    }
    for (const auto& peer : listener.peers_) {
        Require(peer->Wait([&] { return peer->closed_; }), "stress.server_shutdown_timeout");
        peer->DrainSends();
    }
    const auto ended = std::chrono::steady_clock::now();
    const auto after = Measure();
    std::cout << "QUIC_STRESS peers=" << count << " handshake_ms="
              << std::chrono::duration<double, std::milli>(connected - started).count()
              << " total_ms=" << std::chrono::duration<double, std::milli>(ended - started).count()
              << " cpu_ms=" << after.cpu_ms_ - before.cpu_ms_
              << " peak_resident_bytes=" << after.peak_resident_bytes_
              << " limit_rejected=1 reliable_and_datagram=passed in_flight_after_close=0\n";
}
}  // namespace rhythm::quic_probe
