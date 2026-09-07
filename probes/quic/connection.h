// Private candidate peer lifecycle and bounded buffer adapter.
#pragma once
#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <vector>

#include "native_api.h"
#include "receive_inbox.h"
#include "rhythm/cluster/send_queue.h"
#include "rhythm/cluster/stream_decoder.h"
namespace rhythm::quic_probe {
class Connection final {
   public:
    explicit Connection(std::shared_ptr<Api> api) : api_(std::move(api)) {}
    ~Connection() {
        send_queue_.Close();
        if (!connection_.Get()) return;
        api_->Table().ConnectionShutdown(connection_.Get(), QUIC_CONNECTION_SHUTDOWN_FLAG_SILENT,
                                         0);
        // Lifecycle shutdown is owned here, before callback state or send buffers
        // are released. ConnectionClose also aborts streams if shutdown times out.
        if (!Wait([&] { return closed_; })) connection_.Reset();
        stream_.Reset();
        connection_.Reset();
        // No callback can use descriptors after close, even on a timeout path.
        std::lock_guard lock(mutex_);
        for (auto& slot : sends_) slot.completed_ = true;
        DrainCompleted();
    }
    void DrainSends() {
        // Shutdown notification and all send callbacks need not be simultaneous.
        // Native close is the quiescence boundary before asserting final drain.
        stream_.Reset();
        connection_.Reset();
        std::lock_guard lock(mutex_);
        DrainCompleted();
        Require(send_queue_.Stats().in_flight_count_ == 0, "send.completion_not_drained");
    }
    void Send(bool datagram) { SendBytes(payload_, datagram, true); }
    void SendBytes(std::span<const std::uint8_t> payload, bool datagram, bool fin) {
        std::size_t index = 0;
        {
            std::lock_guard lock(mutex_);
            DrainCompleted();
            while (index < sends_.size() && sends_[index].pending_) ++index;
            Require(index < sends_.size(), "send.native_slots");
            const auto channel = datagram ? rhythm::cluster::SendChannel::kRealtime
                                          : rhythm::cluster::SendChannel::kControl;
            Require(send_queue_.Enqueue(channel, rhythm::cluster::SendPayload(payload)) ==
                            rhythm::cluster::QueueResult::kAccepted,
                    "send.queue_budget");
            auto& slot = sends_[index];
            slot.pending_ = send_queue_.Acquire();
            Require(slot.pending_.has_value(), "send.acquire");
            slot.completed_ = false;
            const auto bytes = slot.pending_->payload_.Bytes();
            // MsQuic borrows these immutable bytes through a C API mutable pointer
            // descriptor. Only this adapter casts away const; it never writes them.
            slot.buffers_[1] = {static_cast<std::uint32_t>(bytes.size()),
                                const_cast<std::uint8_t*>(bytes.data())};
            const auto size = static_cast<std::uint32_t>(bytes.size());
            slot.header_ = {static_cast<std::uint8_t>(size >> 24),
                            static_cast<std::uint8_t>(size >> 16),
                            static_cast<std::uint8_t>(size >> 8), static_cast<std::uint8_t>(size)};
            slot.buffers_[0] = {4, slot.header_.data()};
        }
        auto& slot = sends_[index];
        // No adapter lock across a native send: completion may run synchronously.
        const auto status =
                datagram ? api_->Table().DatagramSend(connection_.Get(), &slot.buffers_[1], 1,
                                                      QUIC_SEND_FLAG_NONE, &slot)
                         : api_->Table().StreamSend(stream_.Get(), slot.buffers_.data(), 2,
                                                    fin ? QUIC_SEND_FLAG_FIN : QUIC_SEND_FLAG_NONE,
                                                    &slot);
        if (QUIC_FAILED(status)) {
            std::lock_guard lock(mutex_);
            slot.completed_ = true;  // Failed submission did not transfer the borrow.
            DrainCompleted();
        }
        Check(status, datagram ? "datagram.send" : "stream.send");
    }
    // Owner thread calls with mutex held; callbacks only publish completion bits.
    void DrainCompleted() {
        for (auto& slot : sends_) {
            if (slot.pending_ && slot.completed_) {
                send_queue_.Complete(slot.pending_->ticket_);
                slot.pending_.reset();
            }
        }
    }
    void MarkComplete(void* context) {
        for (auto& slot : sends_)
            if (&slot == context) slot.completed_ = true;
        condition_.notify_all();
    }
    bool Wait(const std::function<bool()>& predicate) {
        std::unique_lock lock(mutex_);
        return condition_.wait_for(lock, std::chrono::seconds(10), predicate);
    }
    std::string State() {
        std::lock_guard lock(mutex_);
        return " connected=" + std::to_string(connected_) + " closed=" + std::to_string(closed_) +
               " certificate_seen=" + std::to_string(certificate_seen_) +
               " certificate_accepted=" + std::to_string(certificate_accepted_) +
               " datagram=" + std::to_string(datagram_enabled_) +
               " transport_status=" + std::to_string(transport_status_) +
               " transport_error=" + std::to_string(transport_error_);
    }
    static QUIC_STATUS QUIC_API StreamCallback(HQUIC, void* context, QUIC_STREAM_EVENT* event) {
        if (!context || !event) return QUIC_STATUS_INVALID_PARAMETER;
        auto& self = *static_cast<Connection*>(context);
        try {
            std::lock_guard lock(self.mutex_);
            if (event->Type == QUIC_STREAM_EVENT_RECEIVE) {
                if (self.bounded_receive_) {
                    const auto status = self.inbox_.Receive(*event);
                    self.condition_.notify_all();
                    return status;
                }
                const auto& received = event->RECEIVE;
                if (received.TotalBufferLength > 1024 - self.stream_bytes_.size())
                    return QUIC_STATUS_BUFFER_TOO_SMALL;
                for (std::uint32_t index = 0; index < received.BufferCount; ++index) {
                    const auto& buffer = received.Buffers[index];
                    if (buffer.Length > 1024 - self.stream_bytes_.size())
                        return QUIC_STATUS_BUFFER_TOO_SMALL;
                    const auto decoded = self.decoder_.Feed({buffer.Buffer, buffer.Length});
                    if (decoded.status_ != rhythm::cluster::StreamFeedStatus::kConsumed ||
                        decoded.consumed_ != buffer.Length)
                        return QUIC_STATUS_BUFFER_TOO_SMALL;
                    while (const auto message = self.decoder_.Take()) {
                        if (message->Bytes().size() > 1024 - self.stream_bytes_.size())
                            return QUIC_STATUS_BUFFER_TOO_SMALL;
                        self.stream_bytes_.insert(self.stream_bytes_.end(),
                                                  message->Bytes().begin(), message->Bytes().end());
                    }
                }
                if ((received.Flags & QUIC_RECEIVE_FLAG_FIN) && !self.decoder_.Finish())
                    return QUIC_STATUS_INVALID_PARAMETER;
                self.condition_.notify_all();
            } else if (event->Type == QUIC_STREAM_EVENT_SEND_COMPLETE) {
                self.MarkComplete(event->SEND_COMPLETE.ClientContext);
            }
            return QUIC_STATUS_SUCCESS;
        } catch (...) {
            return QUIC_STATUS_INTERNAL_ERROR;
        }
    }
    static QUIC_STATUS QUIC_API Callback(HQUIC, void* context, QUIC_CONNECTION_EVENT* event) {
        if (!context || !event) return QUIC_STATUS_INVALID_PARAMETER;
        auto& self = *static_cast<Connection*>(context);
        try {
            std::lock_guard lock(self.mutex_);
            switch (event->Type) {
                case QUIC_CONNECTION_EVENT_CONNECTED:
                    self.connected_ = true;
                    break;
                case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
                    self.closed_ = true;
                    break;
                case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_TRANSPORT:
                    self.transport_status_ = event->SHUTDOWN_INITIATED_BY_TRANSPORT.Status;
                    self.transport_error_ = event->SHUTDOWN_INITIATED_BY_TRANSPORT.ErrorCode;
                    break;
                case QUIC_CONNECTION_EVENT_DATAGRAM_STATE_CHANGED:
                    self.datagram_enabled_ = event->DATAGRAM_STATE_CHANGED.SendEnabled;
                    break;
                case QUIC_CONNECTION_EVENT_DATAGRAM_SEND_STATE_CHANGED:
                    if (QUIC_DATAGRAM_SEND_STATE_IS_FINAL(event->DATAGRAM_SEND_STATE_CHANGED.State))
                        self.MarkComplete(event->DATAGRAM_SEND_STATE_CHANGED.ClientContext);
                    break;
                case QUIC_CONNECTION_EVENT_DATAGRAM_RECEIVED: {
                    const auto& buffer = *event->DATAGRAM_RECEIVED.Buffer;
                    if (buffer.Length > 1024) return QUIC_STATUS_BUFFER_TOO_SMALL;
                    self.datagram_bytes_.assign(buffer.Buffer, buffer.Buffer + buffer.Length);
                    break;
                }
                case QUIC_CONNECTION_EVENT_PEER_STREAM_STARTED:
                    if (self.stream_.Get()) return QUIC_STATUS_INVALID_STATE;
                    self.stream_.Adopt(event->PEER_STREAM_STARTED.Stream);
                    self.api_->Table().SetCallbackHandler(
                            self.stream_.Get(), reinterpret_cast<void*>(StreamCallback), &self);
                    break;
                case QUIC_CONNECTION_EVENT_PEER_CERTIFICATE_RECEIVED: {
                    self.certificate_seen_ = true;
                    // DER is borrowed only during this callback. The fixture pins the
                    // exact certificate bytes, not a mutable machine trust-store entry.
                    const auto* certificate = static_cast<const QUIC_BUFFER*>(
                            event->PEER_CERTIFICATE_RECEIVED.Certificate);
                    self.certificate_accepted_ =
                            certificate && certificate->Buffer &&
                            (self.verify_certificate_
                                     ? self.verify_certificate_(
                                               {certificate->Buffer, certificate->Length})
                                     : certificate->Length == self.expected_certificate_.size() &&
                                               std::equal(self.expected_certificate_.begin(),
                                                          self.expected_certificate_.end(),
                                                          certificate->Buffer));
                    self.condition_.notify_all();
                    return self.certificate_accepted_ ? QUIC_STATUS_SUCCESS
                                                      : QUIC_STATUS_BAD_CERTIFICATE;
                }
                default:
                    break;
            }
            self.condition_.notify_all();
            return QUIC_STATUS_SUCCESS;
        } catch (...) {
            return QUIC_STATUS_INTERNAL_ERROR;
        }
    }
    std::shared_ptr<Api> api_{};
    std::mutex mutex_{};
    std::condition_variable condition_{};
    bool connected_ = false;
    bool closed_ = false;
    bool datagram_enabled_ = false;
    bool certificate_seen_ = false;
    bool certificate_accepted_ = false;
    QUIC_STATUS transport_status_ = QUIC_STATUS_SUCCESS;
    std::uint64_t transport_error_ = 0;
    std::vector<std::uint8_t> expected_certificate_{};
    // Set before ConnectionStart; invoked synchronously with the borrowed DER.
    std::function<bool(std::span<const std::uint8_t>)> verify_certificate_{};
    std::vector<std::uint8_t> datagram_bytes_{};
    std::vector<std::uint8_t> stream_bytes_{};
    std::array<std::uint8_t, 16> payload_{1, 3, 5, 7, 9};
    struct NativeSend {
        std::optional<rhythm::cluster::PendingSend> pending_{};
        std::array<std::uint8_t, 4> header_{};
        // Native descriptors borrow immutable queue payloads until final callbacks.
        std::array<QUIC_BUFFER, 2> buffers_{};
        bool completed_ = false;
    };
    std::array<NativeSend, 16> sends_{};
    rhythm::cluster::SendQueue send_queue_{1};
    rhythm::cluster::StreamDecoder decoder_{rhythm::cluster::SendChannel::kControl};
    bool bounded_receive_ = false;
    ReceiveInbox inbox_{};
    Handle connection_{api_, Kind::kConnection};
    Handle stream_{api_, Kind::kStream};
};
}  // namespace rhythm::quic_probe
