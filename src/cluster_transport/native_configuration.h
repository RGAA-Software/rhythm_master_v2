#pragma once

#include "native_api.h"
#include "rhythm/cluster_auth/endpoint.h"
#include "rhythm/security/identity.h"

namespace rhythm::transport::detail {
QUIC_ADDR NativeAddress(cluster_auth::Endpoint endpoint);
cluster_auth::Endpoint ProjectAddress(const QUIC_ADDR& address);
// Owns a registration and its one role-specific immutable TLS configuration.
class NativeConfiguration final {
   public:
    explicit NativeConfiguration(bool client);
    void LoadHost(const security::HostIdentity& identity);
    void StartListener(const NativeHandle& listener, cluster_auth::Endpoint endpoint);
    void StartConnection(const NativeHandle& connection, cluster_auth::Endpoint endpoint);
    std::shared_ptr<Api> api_ = std::make_shared<Api>();
    NativeHandle registration_{api_, HandleKind::kRegistration};
    NativeHandle configuration_{api_, HandleKind::kConfiguration};
};
}  // namespace rhythm::transport::detail
