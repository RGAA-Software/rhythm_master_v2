#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include "rhythm/cluster_auth/endpoint.h"
#include "rhythm/cluster_auth/room_authority.h"

namespace rhythm::cluster_auth {
struct InvitationLink {
    Invitation invitation_{};
    security::CertificatePin pin_{};
    Endpoint endpoint_{};
};
// Canonical, bounded, unpadded base64url. No query parameters, percent escaping,
// DNS lookup, userinfo or redirect. The complete link is a secret: never log it.
// IPv6 scope-dependent/link-local and multicast endpoints are not supported by
// this schema. A native host adapter chooses an explicit reachable unicast address.
std::optional<std::string> EncodeInvitation(const InvitationLink& link);
std::optional<InvitationLink> DecodeInvitation(std::string_view text);
}  // namespace rhythm::cluster_auth
