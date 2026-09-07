#pragma once

#include "rhythm/cluster_auth/control_codec.h"

namespace rhythm::cluster_auth {
enum class AdmissionState { kAwaiting, kAdmitted, kClosed };
struct AdmissionDecision {
    std::optional<AdmissionMessage> reply_{};
    bool close_ = false;
};
// One local TLS connection's serial admission lifecycle. It owns no transport;
// caller closes native transport on close_, then always calls TransportClosed.
// Only kAdmitted connections can reach further typed participant-message handlers.
class AdmissionGate final {
   public:
    AdmissionGate(std::uint64_t connection, std::int64_t connected_at_us);
    AdmissionGate(const AdmissionGate&) = delete;
    AdmissionGate& operator=(const AdmissionGate&) = delete;
    AdmissionDecision Handle(RoomAuthority& authority, const AdmissionMessage& message,
                             std::int64_t now_us);
    // False means close the connection. Unauthenticated traffic never extends the
    // five-second deadline. Call regularly even when the peer sends no messages.
    bool Tick(std::int64_t now_us);
    void TransportClosed(RoomAuthority& authority);
    AdmissionState State() const { return state_; }
    std::uint64_t Peer() const { return peer_; }

   private:
    AdmissionDecision Reject(AdmissionError error);
    std::uint64_t connection_ = 0;
    std::uint64_t peer_ = 0;
    std::int64_t last_now_us_ = 0;
    std::int64_t deadline_us_ = 0;
    std::int64_t lease_expires_us_ = 0;
    AdmissionState state_ = AdmissionState::kAwaiting;
};
}  // namespace rhythm::cluster_auth
