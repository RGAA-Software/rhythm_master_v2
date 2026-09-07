#include "native_peer.h"

namespace rhythm::transport::detail {
NativeStream::NativeStream(NativePeer& owner, std::shared_ptr<Api> api,
                           cluster::SendChannel channel)
    : owner_(std::ref(owner)),
      handle_(std::move(api), HandleKind::kStream),
      decoder_(channel),
      channel_(channel) {}
QUIC_STATUS QUIC_API NativeStream::Callback(HQUIC, void* context, QUIC_STREAM_EVENT* event) {
    if (!context || !event) return QUIC_STATUS_INVALID_PARAMETER;
    auto& stream = *static_cast<NativeStream*>(context);
    auto& peer = stream.owner_->get();
    try {
        std::lock_guard lock(peer.mutex_);
        if (event->Type == QUIC_STREAM_EVENT_SEND_COMPLETE) {
            peer.sender_.MarkComplete(event->SEND_COMPLETE.ClientContext);
        } else if (event->Type == QUIC_STREAM_EVENT_RECEIVE) {
            const auto total = event->RECEIVE.TotalBufferLength;
            std::uint64_t consumed = 0;
            for (std::uint32_t index = 0; index < event->RECEIVE.BufferCount; ++index) {
                const auto& buffer = event->RECEIVE.Buffers[index];
                const auto result = stream.decoder_.Feed({buffer.Buffer, buffer.Length});
                consumed += result.consumed_;
                if (result.status_ == cluster::StreamFeedStatus::kInvalid ||
                    result.status_ == cluster::StreamFeedStatus::kClosed) {
                    peer.Fault(CloseReason::kProtocol);
                    break;
                }
                if (result.status_ == cluster::StreamFeedStatus::kBackpressure) break;
            }
            event->RECEIVE.TotalBufferLength = consumed;
            stream.paused_ = consumed < total;
            // Control and asset channels are persistent until connection shutdown.
            if (consumed == total && (event->RECEIVE.Flags & QUIC_RECEIVE_FLAG_FIN))
                peer.Fault(CloseReason::kProtocol);
        } else if (event->Type == QUIC_STREAM_EVENT_PEER_SEND_ABORTED ||
                   event->Type == QUIC_STREAM_EVENT_PEER_RECEIVE_ABORTED) {
            if (!peer.closing_) peer.Fault(CloseReason::kProtocol);
        }
        return QUIC_STATUS_SUCCESS;
    } catch (...) {
        std::lock_guard lock(peer.mutex_);
        peer.Fault(CloseReason::kTransport);
        return QUIC_STATUS_INTERNAL_ERROR;
    }
}
NativePeer::NativePeer(std::shared_ptr<Api> api, ConnectionHandle id, bool client)
    : api_(std::move(api)),
      id_(id),
      connection_(api_, HandleKind::kConnection),
      sender_(id.value_),
      client_(client) {}
NativePeer::~NativePeer() { Quiesce(); }
void NativePeer::Fault(CloseReason reason) {
    faulted_ = true;
    if (reason_ == CloseReason::kNone) reason_ = reason;
}
QUIC_STATUS QUIC_API NativePeer::Callback(HQUIC, void* context, QUIC_CONNECTION_EVENT* event) {
    if (!context || !event) return QUIC_STATUS_INVALID_PARAMETER;
    auto& peer = *static_cast<NativePeer*>(context);
    try {
        std::unique_lock lock(peer.mutex_);
        switch (event->Type) {
            case QUIC_CONNECTION_EVENT_CONNECTED:
                peer.connected_ = true;
                break;
            case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
                peer.closed_ = true;
                break;
            case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_PEER:
                if (peer.reason_ == CloseReason::kNone) peer.reason_ = CloseReason::kRemote;
                peer.closing_ = true;
                peer.sender_.Close();
                break;
            case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_TRANSPORT:
                if (peer.reason_ == CloseReason::kNone) peer.reason_ = CloseReason::kTransport;
                peer.closing_ = true;
                peer.sender_.Close();
                break;
            case QUIC_CONNECTION_EVENT_DATAGRAM_STATE_CHANGED:
                peer.maximum_datagram_bytes_ = event->DATAGRAM_STATE_CHANGED.SendEnabled
                                                       ? event->DATAGRAM_STATE_CHANGED.MaxSendLength
                                                       : 0;
                peer.capabilities_changed_ = true;
                break;
            case QUIC_CONNECTION_EVENT_DATAGRAM_SEND_STATE_CHANGED:
                if (QUIC_DATAGRAM_SEND_STATE_IS_FINAL(event->DATAGRAM_SEND_STATE_CHANGED.State))
                    peer.sender_.MarkComplete(event->DATAGRAM_SEND_STATE_CHANGED.ClientContext);
                break;
            case QUIC_CONNECTION_EVENT_DATAGRAM_RECEIVED: {
                const auto& buffer = *event->DATAGRAM_RECEIVED.Buffer;
                if (!buffer.Length || buffer.Length > 1024) {
                    peer.Fault(CloseReason::kProtocol);
                    break;
                }
                peer.realtime_.emplace(std::span<const std::uint8_t>(buffer.Buffer, buffer.Length));
                break;
            }
            case QUIC_CONNECTION_EVENT_PEER_STREAM_STARTED: {
                const bool asset = (event->PEER_STREAM_STARTED.Flags &
                                    QUIC_STREAM_OPEN_FLAG_UNIDIRECTIONAL) != 0;
                auto& stream = asset ? peer.asset_ : peer.control_;
                if (peer.closing_ || asset != peer.client_ || stream.handle_.Get()) {
                    NativeHandle rejected(peer.api_, HandleKind::kStream);
                    rejected.Adopt(event->PEER_STREAM_STARTED.Stream);
                    peer.Fault(CloseReason::kProtocol);
                    lock.unlock();
                    break;
                }
                stream.handle_.Adopt(event->PEER_STREAM_STARTED.Stream);
                peer.api_->Table().SetCallbackHandler(
                        stream.handle_.Get(), reinterpret_cast<void*>(NativeStream::Callback),
                        &stream);
                break;
            }
            case QUIC_CONNECTION_EVENT_PEER_CERTIFICATE_RECEIVED: {
                const auto* certificate = static_cast<const QUIC_BUFFER*>(
                        event->PEER_CERTIFICATE_RECEIVED.Certificate);
                if (!peer.client_ || !certificate || !certificate->Buffer ||
                    !security::VerifyCertificate({certificate->Buffer, certificate->Length},
                                                 peer.pin_)) {
                    peer.Fault(CloseReason::kCertificate);
                    return QUIC_STATUS_BAD_CERTIFICATE;
                }
                break;
            }
            default:
                break;
        }
        return QUIC_STATUS_SUCCESS;
    } catch (...) {
        std::lock_guard lock(peer.mutex_);
        peer.Fault(CloseReason::kTransport);
        return QUIC_STATUS_INTERNAL_ERROR;
    }
}
void NativePeer::OpenStream(cluster::SendChannel channel) {
    auto& stream = channel == cluster::SendChannel::kAsset ? asset_ : control_;
    {
        std::lock_guard lock(mutex_);
        if (stream.handle_.Get()) return;
    }
    NativeHandle handle(api_, HandleKind::kStream);
    Check(api_->Table().StreamOpen(connection_.Get(),
                                   channel == cluster::SendChannel::kAsset
                                           ? QUIC_STREAM_OPEN_FLAG_UNIDIRECTIONAL
                                           : QUIC_STREAM_OPEN_FLAG_NONE,
                                   NativeStream::Callback, &stream, handle.Output()));
    {
        std::lock_guard lock(mutex_);
        if (closing_ || closed_ || faulted_) return;
        stream.handle_ = std::move(handle);
    }
    Check(api_->Table().StreamStart(stream.handle_.Get(), QUIC_STREAM_START_FLAG_IMMEDIATE));
}
cluster::QueueResult NativePeer::Send(cluster::SendChannel channel, cluster::SendPayload payload) {
    {
        std::lock_guard lock(mutex_);
        if (!connected_ || closing_ || closed_ || faulted_ || draining_)
            return cluster::QueueResult::kClosed;
        if ((channel == cluster::SendChannel::kRealtime &&
             (!maximum_datagram_bytes_ || payload.Bytes().size() > maximum_datagram_bytes_)) ||
            (channel == cluster::SendChannel::kAsset && client_) ||
            (channel == cluster::SendChannel::kControl && !client_ && !control_.handle_.Get()))
            return cluster::QueueResult::kInvalid;
    }
    try {
        if ((channel == cluster::SendChannel::kControl && client_) ||
            channel == cluster::SendChannel::kAsset)
            OpenStream(channel);
    } catch (const std::exception&) {
        std::lock_guard lock(mutex_);
        Fault(CloseReason::kTransport);
        return cluster::QueueResult::kClosed;
    }
    std::lock_guard lock(mutex_);
    if (closing_ || closed_ || faulted_) return cluster::QueueResult::kClosed;
    return sender_.Enqueue(channel, std::move(payload));
}
void NativePeer::Pump() {
    bool faulted = false;
    bool drain_complete = false;
    {
        std::lock_guard lock(mutex_);
        faulted = faulted_;
        drain_complete = draining_ &&
                         (sender_.Idle() || std::chrono::steady_clock::now() >= drain_deadline_);
    }
    if (faulted) {
        BeginClose(CloseReason::kTransport);
        return;
    }
    if (drain_complete) {
        BeginClose(CloseReason::kLocal);
        return;
    }
    for (int count = 0; count < 16; ++count) {
        std::optional<std::size_t> slot;
        {
            std::lock_guard lock(mutex_);
            if (!connected_ || closing_ || closed_) return;
            slot = sender_.Prepare();
        }
        if (!slot) return;
        const auto status =
                sender_.Submit(*slot, *api_, connection_, control_.handle_, asset_.handle_);
        if (QUIC_FAILED(status)) {
            std::lock_guard lock(mutex_);
            sender_.Failed(*slot);
            Fault(CloseReason::kTransport);
            return;
        }
    }
}
void NativePeer::DrainAndClose() {
    std::lock_guard lock(mutex_);
    if (closing_ || closed_ || draining_) return;
    draining_ = true;
    drain_deadline_ = std::chrono::steady_clock::now() + std::chrono::seconds(1);
}
void NativePeer::BeginClose(CloseReason reason) {
    {
        std::lock_guard lock(mutex_);
        if (closing_ || closed_ || !connection_.Get()) return;
        closing_ = true;
        if (reason_ == CloseReason::kNone) reason_ = reason;
        sender_.Close();
    }
    api_->Table().ConnectionShutdown(connection_.Get(), QUIC_CONNECTION_SHUTDOWN_FLAG_NONE, 0);
}
void NativePeer::Quiesce() {
    BeginClose(CloseReason::kLocal);
    NativeHandle control;
    NativeHandle asset;
    NativeHandle connection;
    {
        std::lock_guard lock(mutex_);
        closing_ = true;
        control = std::move(control_.handle_);
        asset = std::move(asset_.handle_);
        connection = std::move(connection_);
    }
    // No peer lock across native close: send completion callbacks take that lock.
    control.Reset();
    asset.Reset();
    connection.Reset();
    std::lock_guard lock(mutex_);
    sender_.Quiesced();
    control_.decoder_.Cancel();
    asset_.decoder_.Cancel();
    realtime_.reset();
}
void NativePeer::ResumeReceives() {
    bool control = false;
    bool asset = false;
    {
        std::lock_guard lock(mutex_);
        if (closing_ || closed_ || faulted_) return;
        if (control_.decoder_.QueuedCount() < 64 && control_.decoder_.QueuedBytes() < 65536)
            control = std::exchange(control_.paused_, false);
        if (asset_.decoder_.QueuedCount() < 8 && asset_.decoder_.QueuedBytes() < 262144)
            asset = std::exchange(asset_.paused_, false);
    }
    if (control) Check(api_->Table().StreamReceiveSetEnabled(control_.handle_.Get(), true));
    if (asset) Check(api_->Table().StreamReceiveSetEnabled(asset_.handle_.Get(), true));
}
}  // namespace rhythm::transport::detail
