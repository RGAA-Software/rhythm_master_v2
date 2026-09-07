#include "room_wire.h"

#include <iostream>

#include "rhythm/cluster_auth/admission_gate.h"

namespace rhythm::security::probe {
namespace {
using namespace rhythm::cluster_auth;
using rhythm::quic_probe::Check;
using rhythm::quic_probe::Connection;
using rhythm::quic_probe::Require;
void Send(Connection& peer, const AdmissionMessage& message) {
    const auto packet = EncodeAdmission(message);
    Require(packet.has_value(), "room.encode");
    peer.SendBytes(packet->Bytes(), false, false);
}
AdmissionMessage Receive(Connection& peer) {
    Require(peer.Wait([&] { return peer.inbox_.Count() || peer.closed_; }), "room.receive_timeout");
    std::lock_guard lock(peer.mutex_);
    const auto payload = peer.inbox_.Take();
    Require(payload.has_value(), "room.receive_closed");
    const auto message = DecodeAdmission(payload->Bytes());
    Require(message.has_value(), "room.decode");
    return *message;
}
AdmissionMessage Exchange(Connection& client, Connection& server, AdmissionGate& gate,
                          RoomAuthority& authority, const AdmissionMessage& request,
                          std::int64_t now_us, bool close) {
    Send(client, request);
    const auto decision = gate.Handle(authority, Receive(server), now_us);
    Require(decision.reply_.has_value() && decision.close_ == close, "room.decision");
    Send(server, *decision.reply_);
    return Receive(client);
}
}  // namespace
void RoomWire(Connection& client, Connection& server, bool wrong_secret, bool revoked) {
    const auto& api = client.api_->Table();
    Check(api.StreamOpen(client.connection_.Get(), QUIC_STREAM_OPEN_FLAG_NONE,
                         Connection::StreamCallback, &client, client.stream_.Output()),
          "room.stream_open");
    Check(api.StreamStart(client.stream_.Get(), QUIC_STREAM_START_FLAG_IMMEDIATE),
          "room.stream_start");
    RoomAuthority authority(7, 3);
    auto invitation = authority.IssueInvitation(0, 600'000'000, 10).value();
    if (wrong_secret) invitation.secret_.back() ^= 1;
    if (revoked) authority.RevokeInvitation();
    AdmissionGate gate(1, 0);
    const auto rejected = wrong_secret || revoked;
    const auto response =
            Exchange(client, server, gate, authority, JoinRequest{invitation, 1}, 0, rejected);
    if (rejected) {
        Require(std::holds_alternative<AdmissionRejected>(response) && authority.MemberCount() == 0,
                "room.unauthorized_member");
    } else {
        const auto grant = std::get<SessionGrant>(response);
        Require(grant.peer_ == gate.Peer() && grant.epoch_ == 7 && grant.profiles_ == 1 &&
                        authority.MemberCount() == 1,
                "room.welcome");
        const auto refreshed = Exchange(client, server, gate, authority,
                                        RefreshRequest{grant.peer_, 7, 1}, 60'000'000, false);
        const auto renewed = std::get<SessionGrant>(refreshed);
        Require(renewed.peer_ == grant.peer_ && renewed.credential_ != grant.credential_ &&
                        renewed.expires_at_us_ > grant.expires_at_us_,
                "room.renewal");
        const auto replay = Exchange(client, server, gate, authority,
                                     RefreshRequest{grant.peer_, 7, 1}, 60'000'000, true);
        Require(std::holds_alternative<AdmissionRejected>(replay), "room.replay");
    }
    api.ConnectionShutdown(server.connection_.Get(), QUIC_CONNECTION_SHUTDOWN_FLAG_NONE, 0);
    Require(server.Wait([&] { return server.closed_; }), "room.server_close");
    server.DrainSends();
    gate.TransportClosed(authority);
    Require(client.Wait([&] { return client.closed_; }), "room.client_close");
    client.DrainSends();
    Require(gate.State() == AdmissionState::kClosed &&
                    !authority.AuthorizeControl(gate.Peer(), 1, 7, 2, 60'000'000),
            "room.closed_binding");
    std::cout << "Encrypted admission: wrong_secret=" << wrong_secret << " revoked=" << revoked
              << " join/renew/replay/close checks passed\n";
}
}  // namespace rhythm::security::probe
