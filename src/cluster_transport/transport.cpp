#include "rhythm/transport/transport.h"

#include <algorithm>
#include <limits>
#include <thread>

#include "native_configuration.h"
#include "native_peer.h"

namespace rhythm::transport {
using detail::NativePeer;
class Transport::Impl final {
   public:
    Impl(bool client, std::size_t maximum_connections) : native_(client), client_(client) {
        if (!maximum_connections || maximum_connections > 1000)
            throw std::invalid_argument("transport.connection_limit");
        peers_.resize(maximum_connections);
    }
    ~Impl() { Stop(); }
    void CheckOwner() const {
        if (std::this_thread::get_id() != owner_) throw std::logic_error("transport.owner_thread");
    }
    std::shared_ptr<NativePeer> Find(ConnectionHandle id) {
        std::lock_guard lock(mutex_);
        for (const auto& peer : peers_)
            if (peer && peer->id_ == id) return peer;
        return {};
    }
    // Called while mutex_ is held. IDs are consumed even by failed attempts.
    std::shared_ptr<NativePeer> Reserve() {
        if (stopped_ || next_id_ == std::numeric_limits<std::uint64_t>::max()) return {};
        for (auto& peer : peers_) {
            if (!peer) {
                peer = std::make_shared<NativePeer>(native_.api_, ConnectionHandle{++next_id_},
                                                    client_);
                return peer;
            }
        }
        return {};
    }
    static QUIC_STATUS QUIC_API Accept(HQUIC, void* context, QUIC_LISTENER_EVENT* event) {
        if (!context || !event) return QUIC_STATUS_INVALID_PARAMETER;
        if (event->Type != QUIC_LISTENER_EVENT_NEW_CONNECTION) return QUIC_STATUS_SUCCESS;
        auto& self = *static_cast<Impl*>(context);
        try {
            std::shared_ptr<NativePeer> peer;
            {
                std::lock_guard lock(self.mutex_);
                peer = self.Reserve();
                if (!peer) return QUIC_STATUS_CONNECTION_REFUSED;
                // Publication and adoption are atomic with respect to owner polling.
                peer->connection_.Adopt(event->NEW_CONNECTION.Connection);
                self.native_.api_->Table().SetCallbackHandler(
                        peer->connection_.Get(), reinterpret_cast<void*>(NativePeer::Callback),
                        peer.get());
            }
            const auto status = self.native_.api_->Table().ConnectionSetConfiguration(
                    peer->connection_.Get(), self.native_.configuration_.Get());
            if (QUIC_FAILED(status)) {
                std::lock_guard lock(peer->mutex_);
                peer->Fault(CloseReason::kTransport);
            }
            {
                std::lock_guard lock(peer->mutex_);
                peer->ready_ = true;
            }
            // Once adopted, retain context until owner-side native close, even on failure.
            return QUIC_STATUS_SUCCESS;
        } catch (...) {
            return QUIC_STATUS_OUT_OF_MEMORY;
        }
    }
    void Stop() {
        {
            std::lock_guard lock(mutex_);
            if (stopped_) return;
            stopped_ = true;
        }
        listener_.Reset();  // Quiesce accept before touching adopted connections.
        for (const auto& peer : peers_)
            if (peer) peer->BeginClose(CloseReason::kLocal);
        for (auto& peer : peers_) {
            if (peer) peer->Quiesce();
            peer.reset();
        }
    }
    detail::NativeConfiguration native_{false};
    const std::thread::id owner_ = std::this_thread::get_id();
    std::mutex mutex_{};
    std::vector<std::shared_ptr<NativePeer>> peers_{};
    std::optional<cluster_auth::Endpoint> endpoint_{};
    std::uint64_t next_id_ = 0;
    std::size_t poll_start_ = 0;
    bool client_ = false;
    bool stopped_ = false;
    // Last member: closes callbacks before registry destruction even on failed setup.
    detail::NativeHandle listener_{native_.api_, detail::HandleKind::kListener};
};
Transport::Transport(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
Transport::~Transport() = default;
Transport::Transport(Transport&&) noexcept = default;
Transport& Transport::operator=(Transport&&) noexcept = default;
Transport Transport::Listen(const security::HostIdentity& identity, cluster_auth::Endpoint endpoint,
                            std::size_t maximum_connections) {
    auto impl = std::make_unique<Impl>(false, maximum_connections);
    impl->native_.LoadHost(identity);
    detail::Check(impl->native_.api_->Table().ListenerOpen(
            impl->native_.registration_.Get(), Impl::Accept, impl.get(), impl->listener_.Output()));
    impl->native_.StartListener(impl->listener_, endpoint);
    QUIC_ADDR address{};
    std::uint32_t length = sizeof(address);
    detail::Check(impl->native_.api_->Table().GetParam(
            impl->listener_.Get(), QUIC_PARAM_LISTENER_LOCAL_ADDRESS, &length, &address));
    impl->endpoint_ = detail::ProjectAddress(address);
    return Transport(std::move(impl));
}
Transport Transport::Client(std::size_t maximum_connections) {
    return Transport(std::make_unique<Impl>(true, maximum_connections));
}
std::optional<cluster_auth::Endpoint> Transport::LocalEndpoint() const {
    if (!impl_) return {};
    impl_->CheckOwner();
    return impl_->stopped_ ? std::nullopt : impl_->endpoint_;
}
std::optional<ConnectionHandle> Transport::Connect(cluster_auth::Endpoint endpoint,
                                                   security::CertificatePin pin) {
    if (!impl_) return {};
    impl_->CheckOwner();
    if (!impl_->client_ || !endpoint.port_ ||
        std::none_of(pin.begin(), pin.end(), [](auto byte) { return byte != 0; }))
        return {};
    std::shared_ptr<NativePeer> peer;
    {
        std::lock_guard lock(impl_->mutex_);
        peer = impl_->Reserve();
    }
    if (!peer) return {};
    peer->pin_ = pin;  // Immutable after callback registration.
    try {
        detail::Check(impl_->native_.api_->Table().ConnectionOpen(
                impl_->native_.registration_.Get(), NativePeer::Callback, peer.get(),
                peer->connection_.Output()));
        impl_->native_.StartConnection(peer->connection_, endpoint);
    } catch (...) {
        std::lock_guard lock(peer->mutex_);
        peer->reason_ = CloseReason::kTransport;
        peer->closed_ = true;
    }
    {
        std::lock_guard lock(peer->mutex_);
        peer->ready_ = true;
    }
    return peer->id_;
}
cluster::QueueResult Transport::Send(ConnectionHandle connection, cluster::SendChannel channel,
                                     cluster::SendPayload payload) {
    if (!impl_) return cluster::QueueResult::kClosed;
    impl_->CheckOwner();
    auto peer = impl_->Find(connection);
    return peer ? peer->Send(channel, std::move(payload)) : cluster::QueueResult::kClosed;
}
std::vector<Event> Transport::Poll() {
    std::vector<Event> events;
    if (!impl_) return events;
    impl_->CheckOwner();
    std::vector<std::shared_ptr<NativePeer>> peers;
    {
        std::lock_guard lock(impl_->mutex_);
        if (impl_->stopped_) return events;
        peers = impl_->peers_;
    }
    events.reserve(128);
    std::size_t bytes = 0;
    const auto start = impl_->poll_start_++ % peers.size();
    for (std::size_t index = 0; index < peers.size(); ++index) {
        auto peer = peers[(start + index) % peers.size()];
        if (!peer) continue;
        {
            std::lock_guard lock(peer->mutex_);
            if (!peer->ready_) continue;
        }
        peer->Pump();
        bool closed = false;
        {
            std::lock_guard lock(peer->mutex_);
            if (events.size() >= 128) continue;
            const bool preserve_received =
                    peer->connected_ && !peer->faulted_ &&
                    (!peer->closing_ || peer->reason_ == CloseReason::kRemote);
            if (preserve_received) {
                if (!peer->connected_reported_) {
                    events.push_back({peer->id_, EventKind::kConnected});
                    peer->connected_reported_ = true;
                }
                if (!peer->closed_ && peer->capabilities_changed_ && events.size() < 128) {
                    Event capabilities{peer->id_, EventKind::kCapabilities};
                    capabilities.maximum_datagram_bytes_ =
                            std::min<std::uint16_t>(1024, peer->maximum_datagram_bytes_);
                    events.push_back(std::move(capabilities));
                    peer->capabilities_changed_ = false;
                }
                // Rotate peers, cap one peer's reliable burst, and prioritize control.
                for (int count = 0; count < 16 && events.size() < 128 && bytes + 4096 <= 1048576;
                     ++count) {
                    auto payload = peer->control_.decoder_.Take();
                    if (!payload) break;
                    bytes += payload->Bytes().size();
                    events.push_back({peer->id_, EventKind::kControl, CloseReason::kNone,
                                      std::move(*payload)});
                }
                if (peer->realtime_ && events.size() < 128 && bytes + 1024 <= 1048576) {
                    bytes += peer->realtime_->Bytes().size();
                    events.push_back({peer->id_, EventKind::kRealtime, CloseReason::kNone,
                                      std::move(*peer->realtime_)});
                    peer->realtime_.reset();
                }
                for (int count = 0; count < 4 && events.size() < 128 && bytes + 65536 <= 1048576;
                     ++count) {
                    auto payload = peer->asset_.decoder_.Take();
                    if (!payload) break;
                    bytes += payload->Bytes().size();
                    events.push_back({peer->id_, EventKind::kAsset, CloseReason::kNone,
                                      std::move(*payload)});
                }
            }
            // Native ACK can precede owner polling. A clean peer close must not
            // erase complete messages already acknowledged into our bounded inbox.
            const bool inbox_empty = !peer->control_.decoder_.QueuedCount() &&
                                     !peer->asset_.decoder_.QueuedCount() && !peer->realtime_;
            closed = peer->closed_ && (!preserve_received || inbox_empty) && events.size() < 128;
            if (closed) events.push_back({peer->id_, EventKind::kDisconnected, peer->reason_});
        }
        if (closed) {
            {
                std::lock_guard lock(impl_->mutex_);
                for (auto& slot : impl_->peers_)
                    if (slot == peer) {
                        slot.reset();
                        break;
                    }
            }
            peer->Quiesce();
        } else {
            try {
                peer->ResumeReceives();
            } catch (...) {
                std::lock_guard lock(peer->mutex_);
                peer->Fault(CloseReason::kTransport);
            }
        }
    }
    return events;
}
void Transport::Disconnect(ConnectionHandle connection) {
    if (!impl_) return;
    impl_->CheckOwner();
    auto peer = impl_->Find(connection);
    if (peer) peer->BeginClose(CloseReason::kLocal);
}
void Transport::Stop() {
    if (!impl_) return;
    impl_->CheckOwner();
    impl_->Stop();
}
void Transport::DrainAndDisconnect(ConnectionHandle connection) {
    if (!impl_) return;
    impl_->CheckOwner();
    auto peer = impl_->Find(connection);
    if (peer) peer->DrainAndClose();
}
}  // namespace rhythm::transport
