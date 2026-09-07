#pragma once

#include "rhythm/security/identity.h"

namespace rhythm::security::detail {
// Private bridge for TLS credential adapters only. The borrowed bytes remain
// owned by HostIdentity and must not escape the synchronous credential load.
class IdentityAccess final {
   public:
    static std::span<const std::uint8_t> Pkcs12(const HostIdentity& identity);
};
}  // namespace rhythm::security::detail
