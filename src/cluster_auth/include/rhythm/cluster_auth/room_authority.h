#pragma once

#include <cstdint>
#include <map>
#include <optional>

#include "rhythm/security/identity.h"

namespace rhythm::cluster_auth {
struct Invitation {
    security::Token room_{};
    security::Token secret_{};
    std::uint64_t epoch_ = 0;
    std::uint64_t generation_ = 0;
    // Host monotonic time only. A participant must not compare its own clock to it.
    std::int64_t expires_at_us_ = 0;
};
struct SessionGrant {
    std::uint64_t peer_ = 0;
    std::uint64_t epoch_ = 0;
    security::Token credential_{};
    std::uint32_t profiles_ = 0;
    std::int64_t expires_at_us_ = 0;
    std::uint64_t last_control_sequence_ = 0;
};
enum class AdmissionError {
    kNone = 0,
    kInvalid = 1,
    kLocked = 2,
    kCapacity = 3,
    kRateLimited = 4,
    kIncompatible = 5
};
struct Admission {
    AdmissionError error_ = AdmissionError::kInvalid;
    std::optional<SessionGrant> grant_{};
};
// Serialized room-owner access, after TLS establishment. Connection IDs are
// local, nonzero stable handles assigned by the transport, never peer-supplied or
// reused. The adapter permits only one admission attempt in progress per TLS
// connection and tears down transport before calling Disconnect.
// This grants participant playback rights only, never host/admin/resource-upload rights.
class RoomAuthority final {
   public:
    RoomAuthority(std::uint64_t epoch, std::uint32_t profiles, std::size_t maximum_peers = 100);
    RoomAuthority(const RoomAuthority&) = delete;
    RoomAuthority& operator=(const RoomAuthority&) = delete;
    std::optional<Invitation> IssueInvitation(std::int64_t now_us, std::int64_t lifetime_us,
                                              std::size_t maximum_uses);
    void RevokeInvitation();
    void SetLocked(bool locked);
    Admission Join(const Invitation& invitation, std::uint64_t connection, std::uint32_t profiles,
                   std::int64_t now_us);
    // Reconnect only after Disconnect, within the original lease. Successful
    // resume rotates the credential but preserves the control replay watermark.
    // Locking blocks new members; already admitted members may resume.
    Admission Resume(std::uint64_t peer, const security::Token& credential,
                     std::uint64_t connection, std::int64_t now_us);
    // Existing authenticated connections may renew at most once per minute,
    // before expiry. This supports long shows without long-lived bearer tokens.
    Admission Refresh(std::uint64_t peer, std::uint64_t connection, std::int64_t now_us);
    bool AuthorizeControl(std::uint64_t peer, std::uint64_t connection, std::uint64_t epoch,
                          std::uint64_t sequence, std::int64_t now_us);
    bool Disconnect(std::uint64_t connection);
    bool Kick(std::uint64_t peer);
    bool Tick(std::int64_t now_us);
    void End();
    std::size_t MemberCount() const { return peers_.size(); }
    std::uint64_t Epoch() const { return epoch_; }

   private:
    struct Peer {
        SessionGrant grant_{};
        std::uint64_t connection_ = 0;
        std::int64_t refresh_after_us_ = 0;
    };
    bool Advance(std::int64_t now_us);
    bool ConsumeAttempt();
    bool ConnectionAvailable(std::uint64_t connection) const;
    security::Token room_{};
    std::map<std::uint64_t, Peer> peers_{};
    std::optional<Invitation> invitation_{};
    std::optional<std::int64_t> now_us_{};
    std::uint64_t epoch_ = 0;
    std::uint64_t next_peer_ = 0;
    std::uint64_t invitation_generation_ = 0;
    std::uint32_t profiles_ = 0;
    std::size_t maximum_peers_ = 100;
    std::size_t remaining_uses_ = 0;
    std::int64_t attempt_credit_ = 20'000'000;
    std::int64_t expiry_check_at_us_ = std::int64_t{1} << 52;
    bool locked_ = false;
    bool ended_ = false;
};
}  // namespace rhythm::cluster_auth
