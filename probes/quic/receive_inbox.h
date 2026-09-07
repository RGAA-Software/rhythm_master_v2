// Private MsQuic receive adapter; caller serializes access with its peer mutex.
#pragma once

#include <algorithm>

#include "native_api.h"
#include "rhythm/cluster/stream_decoder.h"

namespace rhythm::quic_probe {
class ReceiveInbox final {
   public:
    QUIC_STATUS Receive(QUIC_STREAM_EVENT& event) {
        const auto total = event.RECEIVE.TotalBufferLength;
        std::uint64_t consumed = 0;
        for (std::uint32_t index = 0; index < event.RECEIVE.BufferCount; ++index) {
            const auto& buffer = event.RECEIVE.Buffers[index];
            const auto result = decoder_.Feed({buffer.Buffer, buffer.Length});
            consumed += result.consumed_;
            peak_bytes_ = (std::max)(peak_bytes_, decoder_.QueuedBytes());
            peak_count_ = (std::max)(peak_count_, decoder_.QueuedCount());
            if (result.status_ == rhythm::cluster::StreamFeedStatus::kInvalid ||
                result.status_ == rhythm::cluster::StreamFeedStatus::kClosed)
                return QUIC_STATUS_INVALID_PARAMETER;
            if (result.status_ == rhythm::cluster::StreamFeedStatus::kBackpressure) break;
        }
        Require(consumed <= total, "receive.accounting");
        event.RECEIVE.TotalBufferLength = consumed;
        if (consumed != total) {
            // SUCCESS with a partial count disables receive callbacks. MsQuic
            // retains the suffix; no borrowed buffer is stored or copied here.
            paused_ = true;
            ++pauses_;
        } else if (event.RECEIVE.Flags & QUIC_RECEIVE_FLAG_FIN) {
            if (!decoder_.Finish()) return QUIC_STATUS_INVALID_PARAMETER;
            finished_ = true;
        }
        return QUIC_STATUS_SUCCESS;
    }
    std::optional<rhythm::cluster::SendPayload> Take() { return decoder_.Take(); }
    // After draining, owner takes this flag under the mutex, then calls native
    // StreamReceiveSetEnabled outside the mutex to avoid synchronous reentrancy.
    bool TakeResume() { return std::exchange(paused_, false); }
    bool Paused() const { return paused_; }
    bool Finished() const { return finished_; }
    std::size_t Count() const { return decoder_.QueuedCount(); }
    std::size_t PeakBytes() const { return peak_bytes_; }
    std::size_t PeakCount() const { return peak_count_; }
    std::size_t Pauses() const { return pauses_; }

   private:
    rhythm::cluster::StreamDecoder decoder_{rhythm::cluster::SendChannel::kControl};
    std::size_t peak_bytes_ = 0;
    std::size_t peak_count_ = 0;
    std::size_t pauses_ = 0;
    bool paused_ = false;
    bool finished_ = false;
};
}  // namespace rhythm::quic_probe
