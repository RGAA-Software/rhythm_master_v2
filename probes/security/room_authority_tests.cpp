#include <iostream>
#include <stdexcept>

#include "rhythm/cluster_auth/room_authority.h"

namespace {
void Check(bool value) {
    if (!value) throw std::runtime_error("room.authority_contract");
}
using namespace rhythm::cluster_auth;
void Membership() {
    RoomAuthority room(7, 3, 2);
    const auto invitation = room.IssueInvitation(0, 60'000'000, 3).value();
    auto first = room.Join(invitation, 10, 1, 0).grant_.value();
    auto second = room.Join(invitation, 20, 7, 0).grant_.value();
    Check(first.peer_ != second.peer_ && first.credential_ != second.credential_);
    Check(second.profiles_ == 3 && room.MemberCount() == 2);
    Check(room.Join(invitation, 30, 1, 0).error_ == AdmissionError::kCapacity);
    Check(!room.Join(invitation, 10, 1, 0).grant_);
    Check(room.AuthorizeControl(first.peer_, 10, 7, 1, 0));
    Check(!room.AuthorizeControl(first.peer_, 20, 7, 2, 0));
    Check(!room.AuthorizeControl(first.peer_, 10, 6, 2, 0));
    Check(!room.AuthorizeControl(first.peer_, 10, 7, 1, 0));
    Check(!room.Resume(first.peer_, first.credential_, 11, 0).grant_);
    Check(room.Disconnect(10));
    Check(!room.AuthorizeControl(first.peer_, 10, 7, 2, 0));
    room.SetLocked(true);
    auto resumed = room.Resume(first.peer_, first.credential_, 11, 0).grant_.value();
    Check(resumed.credential_ != first.credential_ && resumed.last_control_sequence_ == 1);
    Check(resumed.expires_at_us_ == first.expires_at_us_);
    Check(!room.AuthorizeControl(first.peer_, 11, 7, 1, 0));
    Check(room.AuthorizeControl(first.peer_, 11, 7, 2, 0));
    Check(room.Disconnect(11));
    Check(!room.Resume(first.peer_, first.credential_, 12, 0).grant_);
    Check(room.Resume(first.peer_, resumed.credential_, 12, 0).grant_.has_value());
    Check(room.Join(invitation, 30, 1, 0).error_ == AdmissionError::kLocked);
    Check(room.Kick(first.peer_));
    Check(!room.AuthorizeControl(first.peer_, 12, 7, 3, 0));
    Check(!room.Resume(first.peer_, resumed.credential_, 13, 0).grant_);
    room.SetLocked(false);
    auto third = room.Join(invitation, 30, 1, 0).grant_.value();
    Check(third.peer_ > second.peer_);
    room.Kick(second.peer_);
    Check(!room.Join(invitation, 40, 1, 0).grant_);  // Three uses exhausted.
    room.End();
    Check(room.MemberCount() == 0 && !room.Tick(1));
    Check(!room.IssueInvitation(1, 1, 1));
}
void RevocationAndTime() {
    RoomAuthority room(8, 1);
    auto old = room.IssueInvitation(0, 1'000'000, 10).value();
    auto current = room.IssueInvitation(0, 1'000'000, 10).value();
    Check(current.generation_ > old.generation_ && current.secret_ != old.secret_);
    Check(!room.Join(old, 1, 1, 0).grant_);
    auto changed = current;
    changed.secret_.front() ^= 1;
    Check(!room.Join(changed, 1, 1, 0).grant_);
    changed = current;
    changed.room_.back() ^= 1;
    Check(!room.Join(changed, 1, 1, 0).grant_);
    changed = current;
    ++changed.epoch_;
    Check(!room.Join(changed, 1, 1, 0).grant_);
    Check(room.Join(current, 1, 2, 0).error_ == AdmissionError::kIncompatible);
    auto peer = room.Join(current, 1, 1, 0).grant_.value();
    room.RevokeInvitation();
    Check(!room.Join(current, 2, 1, 0).grant_);
    Check(room.AuthorizeControl(peer.peer_, 1, 8, 1, 1));
    Check(!room.AuthorizeControl(peer.peer_, 1, 8, 2, 0));
    Check(room.AuthorizeControl(peer.peer_, 1, 8, 2, 1));
    auto expires = room.IssueInvitation(1, 999'999, 10).value();
    Check(!room.Join(expires, 2, 1, 1'000'000).grant_);
    Check(room.AuthorizeControl(peer.peer_, 1, 8, 3, peer.expires_at_us_ - 1));
    Check(!room.AuthorizeControl(peer.peer_, 1, 8, 4, peer.expires_at_us_));
    Check(room.MemberCount() == 0);
    Check(!room.Resume(peer.peer_, peer.credential_, 2, peer.expires_at_us_).grant_);
    Check(!room.IssueInvitation(peer.expires_at_us_, 600'000'001, 1));
    Check(!room.IssueInvitation(peer.expires_at_us_, 1, 1001));
}
void RateAndCapacity() {
    RoomAuthority room(1, 1, 1000);
    const auto invitation = room.IssueInvitation(0, 600'000'000, 1000).value();
    Invitation invalid;
    for (int count = 0; count < 20; ++count)
        Check(room.Join(invalid, 1, 1, 0).error_ == AdmissionError::kInvalid);
    Check(room.Join(invitation, 1, 1, 0).error_ == AdmissionError::kRateLimited);
    Check(room.Join(invitation, 1, 1, 199'999).error_ == AdmissionError::kRateLimited);
    Check(room.Join(invitation, 1, 1, 200'000).grant_.has_value());
    for (std::uint64_t peer = 2; peer <= 1000; ++peer)
        Check(room.Join(invitation, peer, 1, static_cast<std::int64_t>(peer) * 200'000)
                      .grant_.has_value());
    Check(room.MemberCount() == 1000);
    Check(room.Tick(2'000'000'000) && room.MemberCount() == 0);
}
void LongSession() {
    RoomAuthority room(1, 1);
    auto invitation = room.IssueInvitation(0, 60'000'000, 1).value();
    auto grant = room.Join(invitation, 1, 1, 0).grant_.value();
    Check(room.Refresh(grant.peer_, 1, 59'999'999).error_ == AdmissionError::kRateLimited);
    Check(!room.Refresh(grant.peer_, 2, 60'000'000).grant_);
    const auto old_credential = grant.credential_;
    for (std::int64_t minute = 1; minute <= 180; ++minute) {
        const auto now = minute * 60'000'000;
        Check(room.AuthorizeControl(grant.peer_, 1, 1, static_cast<std::uint64_t>(minute), now));
        auto renewed = room.Refresh(grant.peer_, 1, now).grant_.value();
        Check(renewed.credential_ != grant.credential_ &&
              renewed.expires_at_us_ > grant.expires_at_us_);
        Check(renewed.last_control_sequence_ == static_cast<std::uint64_t>(minute));
        Check(room.Refresh(grant.peer_, 1, now).error_ == AdmissionError::kRateLimited);
        grant = renewed;
    }
    Check(room.Disconnect(1));
    Check(!room.Resume(grant.peer_, old_credential, 2, 180 * 60'000'000LL).grant_);
    Check(room.Resume(grant.peer_, grant.credential_, 2, 180 * 60'000'000LL).grant_.has_value());
    Check(!room.Refresh(grant.peer_, 2, grant.expires_at_us_).grant_);
}
}  // namespace
int main() {
    try {
        Membership();
        RevocationAndTime();
        RateAndCapacity();
        LongSession();
        std::cout << "room authority passed: invitation limits/revocation, binding, reconnect "
                     "rotation, replay, lock/kick, expiry and bounded admission\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
