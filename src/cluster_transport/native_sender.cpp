#include "native_sender.h"

namespace rhythm::transport::detail {
void NativeSender::Drain() {
    for (auto& slot : slots_) {
        if (slot.pending_ && slot.completed_) {
            queue_.Complete(slot.pending_->ticket_);
            slot.pending_.reset();
        }
    }
}
cluster::QueueResult NativeSender::Enqueue(cluster::SendChannel channel,
                                           cluster::SendPayload payload) {
    Drain();
    return queue_.Enqueue(channel, std::move(payload));
}
std::optional<std::size_t> NativeSender::Prepare() {
    Drain();
    for (std::size_t index = 0; index < slots_.size(); ++index) {
        auto& slot = slots_[index];
        if (slot.pending_) continue;
        slot.pending_ = queue_.Acquire();
        if (!slot.pending_) return std::nullopt;
        slot.completed_ = false;
        const auto bytes = slot.pending_->payload_.Bytes();
        const auto size = static_cast<std::uint32_t>(bytes.size());
        slot.header_ = {static_cast<std::uint8_t>(size >> 24),
                        static_cast<std::uint8_t>(size >> 16), static_cast<std::uint8_t>(size >> 8),
                        static_cast<std::uint8_t>(size)};
        slot.buffers_[0] = {4, slot.header_.data()};
        // MsQuic's descriptor is mutable, but it only reads this application data.
        slot.buffers_[1] = {size, const_cast<std::uint8_t*>(bytes.data())};
        return index;
    }
    return std::nullopt;
}
QUIC_STATUS NativeSender::Submit(std::size_t index, const Api& api, const NativeHandle& connection,
                                 const NativeHandle& control, const NativeHandle& asset) {
    auto& slot = slots_.at(index);
    switch (slot.pending_->channel_) {
        case cluster::SendChannel::kRealtime:
            return api.Table().DatagramSend(connection.Get(), &slot.buffers_[1], 1,
                                            QUIC_SEND_FLAG_NONE, &slot);
        case cluster::SendChannel::kControl:
            return api.Table().StreamSend(control.Get(), slot.buffers_.data(), 2,
                                          QUIC_SEND_FLAG_NONE, &slot);
        case cluster::SendChannel::kAsset:
            return api.Table().StreamSend(asset.Get(), slot.buffers_.data(), 2, QUIC_SEND_FLAG_NONE,
                                          &slot);
    }
    return QUIC_STATUS_INVALID_PARAMETER;
}
void NativeSender::MarkComplete(void* context) {
    for (auto& slot : slots_)
        if (&slot == context) slot.completed_ = true;
}
void NativeSender::Failed(std::size_t index) {
    slots_.at(index).completed_ = true;
    Drain();
}
void NativeSender::Close() { queue_.Close(); }
bool NativeSender::Idle() {
    Drain();
    const auto stats = queue_.Stats();
    return !stats.pending_bytes_ && !stats.in_flight_count_;
}
void NativeSender::Quiesced() {
    // Called only after native stream/connection close: no SDK borrow remains.
    queue_.Close();
    for (auto& slot : slots_) slot.completed_ = true;
    Drain();
}
}  // namespace rhythm::transport::detail
