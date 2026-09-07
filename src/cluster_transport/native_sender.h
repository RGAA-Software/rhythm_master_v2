#pragma once

#include "native_api.h"
#include "rhythm/cluster/send_queue.h"

namespace rhythm::transport::detail {
// Peer mutex serializes queue/slot changes. Descriptors remain stable until the
// final native callback; native send is always called outside that mutex.
class NativeSender final {
   public:
    explicit NativeSender(std::uint64_t generation) : queue_(generation) {}
    cluster::QueueResult Enqueue(cluster::SendChannel channel, cluster::SendPayload payload);
    std::optional<std::size_t> Prepare();
    QUIC_STATUS Submit(std::size_t index, const Api& api, const NativeHandle& connection,
                       const NativeHandle& control, const NativeHandle& asset);
    void MarkComplete(void* context);
    void Failed(std::size_t index);
    void Close();
    void Quiesced();
    bool Idle();

   private:
    void Drain();
    struct Slot {
        std::optional<cluster::PendingSend> pending_{};
        std::array<std::uint8_t, 4> header_{};
        // C API descriptors borrow queue-owned immutable data, never temporary buffers.
        std::array<QUIC_BUFFER, 2> buffers_{};
        bool completed_ = false;
    };
    cluster::SendQueue queue_{1};
    std::array<Slot, 16> slots_{};
};
}  // namespace rhythm::transport::detail
