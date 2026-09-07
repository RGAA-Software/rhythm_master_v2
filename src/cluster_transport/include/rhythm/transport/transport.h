#pragma once

#include <memory>
#include <optional>
#include <vector>

#include "rhythm/cluster/send_queue.h"
#include "rhythm/cluster_auth/endpoint.h"
#include "rhythm/security/identity.h"

namespace rhythm::transport {
struct ConnectionHandle {
    std::uint64_t value_ = 0;
    bool operator==(const ConnectionHandle&) const = default;
};
enum class EventKind { kConnected, kDisconnected, kCapabilities, kControl, kRealtime, kAsset };
enum class CloseReason { kNone, kLocal, kRemote, kCertificate, kProtocol, kTransport };
struct Event {
    ConnectionHandle connection_{};
    EventKind kind_ = EventKind::kConnected;
    CloseReason reason_ = CloseReason::kNone;
    cluster::SendPayload payload_{};
    std::uint16_t maximum_datagram_bytes_ = 0;
};
// Owner-thread service with private native callbacks. Poll publishes bounded
// immutable values; it never calls graph, UI, authority or renderer code.
// At most 128 events / 1 MiB of payloads per poll. Connection handles are local,
// never reused, and are unrelated to authenticated room member IDs.
// Candidate adapter: currently built by its isolated probe, not adopted by apps.
class Transport final {
   public:
    static Transport Listen(const security::HostIdentity& identity, cluster_auth::Endpoint endpoint,
                            std::size_t maximum_connections = 100);
    static Transport Client(std::size_t maximum_connections = 1);
    ~Transport();
    Transport(Transport&&) noexcept;
    Transport& operator=(Transport&&) noexcept;
    Transport(const Transport&) = delete;
    Transport& operator=(const Transport&) = delete;
    std::optional<cluster_auth::Endpoint> LocalEndpoint() const;
    std::optional<ConnectionHandle> Connect(cluster_auth::Endpoint endpoint,
                                            security::CertificatePin pin);
    // Only after kConnected. kRealtime also requires a nonzero capabilities event.
    // kAsset is host-to-participant only; application authentication is still required.
    cluster::QueueResult Send(ConnectionHandle connection, cluster::SendChannel channel,
                              cluster::SendPayload payload);
    std::vector<Event> Poll();
    void Disconnect(ConnectionHandle connection);
    // Rejects new sends, drains accepted sends for at most one second, then closes.
    // Continue Poll while draining. Stop/destruction always closes immediately.
    void DrainAndDisconnect(ConnectionHandle connection);
    // Stops listener and all connections, quiesces callbacks, releases native borrows.
    void Stop();

   private:
    class Impl;
    explicit Transport(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_{};
};
}  // namespace rhythm::transport
