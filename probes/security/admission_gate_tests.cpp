#include <iostream>
#include <limits>
#include <stdexcept>

#include "rhythm/cluster_auth/admission_gate.h"

namespace {
void Check(bool value) {
    if (!value) throw std::runtime_error("admission.gate_contract");
}
}  // namespace
int main() {
    using namespace rhythm::cluster_auth;
    try {
        RoomAuthority room(7, 1);
        const auto invitation = room.IssueInvitation(0, 600'000'000, 10).value();
        AdmissionGate first(1, 0);
        const auto accepted = first.Handle(room, JoinRequest{invitation, 1}, 0);
        Check(!accepted.close_ && first.State() == AdmissionState::kAdmitted);
        const auto grant = std::get<SessionGrant>(accepted.reply_.value());
        const auto early = first.Handle(room, RefreshRequest{grant.peer_, 7, 1}, 1);
        Check(!early.close_ &&
              std::get<AdmissionRejected>(*early.reply_).error_ == AdmissionError::kRateLimited);
        const auto renewed = first.Handle(room, RefreshRequest{grant.peer_, 7, 2}, 60'000'000);
        Check(!renewed.close_ && std::holds_alternative<SessionGrant>(*renewed.reply_));
        Check(first.Handle(room, RefreshRequest{grant.peer_, 7, 2}, 60'000'000).close_);
        first.TransportClosed(room);
        Check(!room.AuthorizeControl(grant.peer_, 1, 7, 3, 60'000'000));
        AdmissionGate wrong_epoch(2, 60'000'000);
        Check(wrong_epoch.Handle(room, ResumeRequest{grant.peer_, 8, grant.credential_}, 60'000'000)
                      .close_);
        AdmissionGate resumed(3, 60'000'000);
        auto resume = resumed.Handle(
                room,
                ResumeRequest{grant.peer_, 7, std::get<SessionGrant>(*renewed.reply_).credential_},
                60'000'000);
        Check(!resume.close_ && resumed.Peer() == grant.peer_);
        Check(resumed.Handle(room, JoinRequest{invitation, 1}, 60'000'000).close_);
        resumed.TransportClosed(room);
        AdmissionGate timed_out(4, 60'000'000);
        Check(timed_out.Tick(64'999'999));
        Check(timed_out.Handle(room, JoinRequest{invitation, 1}, 65'000'000).close_);
        Check(timed_out.Peer() == 0);
        AdmissionGate wrong_direction(5, 65'000'000);
        Check(wrong_direction.Handle(room, grant, 65'000'000).close_);
        AdmissionGate rollback(6, 65'000'000);
        Check(!rollback.Tick(64'999'999));
        Check(room.MemberCount() == 1);
        AdmissionGate expires(7, 65'000'000);
        const auto expiring = expires.Handle(room, JoinRequest{invitation, 1}, 65'000'000);
        const auto end = std::get<SessionGrant>(*expiring.reply_).expires_at_us_;
        Check(expires.Tick(end - 1) && !expires.Tick(end));
        expires.TransportClosed(room);
        bool overflow_rejected = false;
        try {
            AdmissionGate invalid(8, std::numeric_limits<std::int64_t>::max());
        } catch (const std::invalid_argument&) {
            overflow_rejected = true;
        }
        Check(overflow_rejected);
        std::cout << "admission gate passed: direction, TLS connection lifecycle, timeout, "
                     "renewal, duplicate join and replay closure\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
