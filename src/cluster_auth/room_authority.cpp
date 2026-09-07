#include "rhythm/cluster_auth/room_authority.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace rhythm::cluster_auth {
namespace {
constexpr std::int64_t kMaximumTime = std::int64_t{1} << 52;
constexpr std::int64_t kMaximumInvitationLife = 600'000'000;
constexpr std::int64_t kSessionLife = 1'800'000'000;
constexpr std::int64_t kAttemptCost = 1'000'000;
constexpr std::int64_t kMaximumCredit = 20 * kAttemptCost;
Admission Failure(AdmissionError error) { return {error, std::nullopt}; }
}  // namespace
RoomAuthority::RoomAuthority(std::uint64_t epoch, std::uint32_t profiles, std::size_t maximum_peers)
    : epoch_(epoch), profiles_(profiles), maximum_peers_(maximum_peers) {
    if (!epoch || !profiles || !maximum_peers || maximum_peers > 1000)
        throw std::invalid_argument("room.policy");
    room_ = security::GenerateToken();
}
bool RoomAuthority::Advance(std::int64_t now_us) {
    if (ended_ || now_us < 0 || now_us > kMaximumTime || (now_us_ && now_us < *now_us_))
        return false;
    if (now_us_) {
        const auto elapsed = std::min(now_us - *now_us_, kMaximumCredit);
        attempt_credit_ = std::min(kMaximumCredit, attempt_credit_ + elapsed * 5);
    }
    now_us_ = now_us;
    if (now_us >= expiry_check_at_us_) {
        std::erase_if(peers_, [now_us](const auto& entry) {
            return now_us >= entry.second.grant_.expires_at_us_;
        });
        expiry_check_at_us_ = kMaximumTime;
        for (const auto& [id, peer] : peers_) {
            static_cast<void>(id);
            expiry_check_at_us_ = std::min(expiry_check_at_us_, peer.grant_.expires_at_us_);
        }
    }
    if (invitation_ && now_us >= invitation_->expires_at_us_) RevokeInvitation();
    return true;
}
bool RoomAuthority::ConsumeAttempt() {
    if (attempt_credit_ < kAttemptCost) return false;
    attempt_credit_ -= kAttemptCost;
    return true;
}
bool RoomAuthority::ConnectionAvailable(std::uint64_t connection) const {
    return connection &&
           std::none_of(peers_.begin(), peers_.end(), [connection](const auto& entry) {
               return entry.second.connection_ == connection;
           });
}
std::optional<Invitation> RoomAuthority::IssueInvitation(std::int64_t now_us,
                                                         std::int64_t lifetime_us,
                                                         std::size_t maximum_uses) {
    if (lifetime_us <= 0 || lifetime_us > kMaximumInvitationLife || !maximum_uses ||
        maximum_uses > 1000 || now_us > kMaximumTime - lifetime_us ||
        invitation_generation_ == std::numeric_limits<std::uint64_t>::max() || !Advance(now_us))
        return std::nullopt;
    Invitation invitation{room_, security::GenerateToken(), epoch_, invitation_generation_ + 1,
                          now_us + lifetime_us};
    invitation_ = invitation;
    ++invitation_generation_;
    remaining_uses_ = maximum_uses;
    return invitation;
}
void RoomAuthority::RevokeInvitation() {
    invitation_.reset();
    remaining_uses_ = 0;
}
void RoomAuthority::SetLocked(bool locked) { locked_ = locked; }
Admission RoomAuthority::Join(const Invitation& invitation, std::uint64_t connection,
                              std::uint32_t profiles, std::int64_t now_us) {
    if (!Advance(now_us)) return Failure(AdmissionError::kInvalid);
    if (!ConsumeAttempt()) return Failure(AdmissionError::kRateLimited);
    if (locked_) return Failure(AdmissionError::kLocked);
    if (!ConnectionAvailable(connection) || !invitation_ || !remaining_uses_ ||
        invitation.epoch_ != epoch_ || invitation.generation_ != invitation_->generation_ ||
        invitation.expires_at_us_ != invitation_->expires_at_us_ ||
        !security::TokensEqual(invitation.room_, room_) ||
        !security::TokensEqual(invitation.secret_, invitation_->secret_))
        return Failure(AdmissionError::kInvalid);
    if (!(profiles & profiles_)) return Failure(AdmissionError::kIncompatible);
    if (peers_.size() >= maximum_peers_) return Failure(AdmissionError::kCapacity);
    if (next_peer_ == std::numeric_limits<std::uint64_t>::max() ||
        now_us > kMaximumTime - kSessionLife)
        return Failure(AdmissionError::kInvalid);
    SessionGrant grant{
            next_peer_ + 1,        epoch_, security::GenerateToken(), profiles & profiles_,
            now_us + kSessionLife, 0};
    peers_.emplace(grant.peer_, Peer{grant, connection, now_us + 60'000'000});
    expiry_check_at_us_ = std::min(expiry_check_at_us_, grant.expires_at_us_);
    ++next_peer_;
    --remaining_uses_;
    return {AdmissionError::kNone, grant};
}
Admission RoomAuthority::Resume(std::uint64_t peer, const security::Token& credential,
                                std::uint64_t connection, std::int64_t now_us) {
    if (!Advance(now_us)) return Failure(AdmissionError::kInvalid);
    if (!ConsumeAttempt()) return Failure(AdmissionError::kRateLimited);
    const auto found = peers_.find(peer);
    if (!ConnectionAvailable(connection) || found == peers_.end() || found->second.connection_ ||
        !security::TokensEqual(credential, found->second.grant_.credential_))
        return Failure(AdmissionError::kInvalid);
    auto renewed = security::GenerateToken();
    found->second.grant_.credential_ = renewed;
    found->second.connection_ = connection;
    return {AdmissionError::kNone, found->second.grant_};
}
Admission RoomAuthority::Refresh(std::uint64_t peer, std::uint64_t connection,
                                 std::int64_t now_us) {
    if (!Advance(now_us) || !connection || now_us > kMaximumTime - kSessionLife)
        return Failure(AdmissionError::kInvalid);
    const auto found = peers_.find(peer);
    if (found == peers_.end() || found->second.connection_ != connection)
        return Failure(AdmissionError::kInvalid);
    if (now_us < found->second.refresh_after_us_) return Failure(AdmissionError::kRateLimited);
    auto credential = security::GenerateToken();
    found->second.grant_.credential_ = credential;
    found->second.grant_.expires_at_us_ = now_us + kSessionLife;
    found->second.refresh_after_us_ = now_us + 60'000'000;
    return {AdmissionError::kNone, found->second.grant_};
}
bool RoomAuthority::AuthorizeControl(std::uint64_t peer, std::uint64_t connection,
                                     std::uint64_t epoch, std::uint64_t sequence,
                                     std::int64_t now_us) {
    if (!Advance(now_us) || epoch != epoch_ || !connection || !sequence) return false;
    const auto found = peers_.find(peer);
    if (found == peers_.end() || found->second.connection_ != connection ||
        sequence <= found->second.grant_.last_control_sequence_)
        return false;
    found->second.grant_.last_control_sequence_ = sequence;
    return true;
}
bool RoomAuthority::Disconnect(std::uint64_t connection) {
    if (!connection) return false;
    for (auto& [id, peer] : peers_) {
        static_cast<void>(id);
        if (peer.connection_ == connection) {
            peer.connection_ = 0;
            return true;
        }
    }
    return false;
}
bool RoomAuthority::Kick(std::uint64_t peer) { return peers_.erase(peer) != 0; }
bool RoomAuthority::Tick(std::int64_t now_us) { return Advance(now_us); }
void RoomAuthority::End() {
    ended_ = true;
    peers_.clear();
    RevokeInvitation();
}
}  // namespace rhythm::cluster_auth
