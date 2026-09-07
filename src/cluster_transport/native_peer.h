#pragma once

#include <chrono>
#include <functional>
#include <mutex>

#include "native_sender.h"
#include "rhythm/cluster/stream_decoder.h"
#include "rhythm/transport/transport.h"

namespace rhythm::transport::detail {
class NativePeer;
class NativeStream final {
   public:
    NativeStream(NativePeer& owner, std::shared_ptr<Api> api, cluster::SendChannel channel);
    static QUIC_STATUS QUIC_API Callback(HQUIC, void* context, QUIC_STREAM_EVENT* event);
    std::optional<std::reference_wrapper<NativePeer>> owner_{};
    NativeHandle handle_{};
    cluster::StreamDecoder decoder_{cluster::SendChannel::kControl};
    cluster::SendChannel channel_ = cluster::SendChannel::kControl;
    bool paused_ = false;
};
// Registry and callback context share ownership. All mutable callback/owner state
// below is protected by mutex_; SDK calls run outside it except callback setup.
class NativePeer final {
   public:
    NativePeer(std::shared_ptr<Api> api, ConnectionHandle id, bool client);
    ~NativePeer();
    NativePeer(const NativePeer&) = delete;
    NativePeer& operator=(const NativePeer&) = delete;
    static QUIC_STATUS QUIC_API Callback(HQUIC, void* context, QUIC_CONNECTION_EVENT* event);
    cluster::QueueResult Send(cluster::SendChannel channel, cluster::SendPayload payload);
    void Pump();
    void BeginClose(CloseReason reason);
    void DrainAndClose();
    void Quiesce();
    void Fault(CloseReason reason);  // Requires mutex_.
    void ResumeReceives();
    void OpenStream(cluster::SendChannel channel);

    std::shared_ptr<Api> api_{};
    ConnectionHandle id_{};
    std::mutex mutex_{};
    NativeHandle connection_{};
    NativeSender sender_{1};
    NativeStream control_{*this, api_, cluster::SendChannel::kControl};
    NativeStream asset_{*this, api_, cluster::SendChannel::kAsset};
    security::CertificatePin pin_{};
    std::optional<cluster::SendPayload> realtime_{};
    CloseReason reason_ = CloseReason::kNone;
    std::uint16_t maximum_datagram_bytes_ = 0;
    bool client_ = false;
    bool ready_ = false;
    bool connected_ = false;
    bool connected_reported_ = false;
    bool capabilities_changed_ = false;
    bool closing_ = false;
    bool closed_ = false;
    bool faulted_ = false;
    bool draining_ = false;
    std::chrono::steady_clock::time_point drain_deadline_{};
};
}  // namespace rhythm::transport::detail
