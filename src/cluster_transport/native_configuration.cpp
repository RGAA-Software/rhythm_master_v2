#include "native_configuration.h"

#include <algorithm>
#include <array>
#include <cstring>

#include "identity_access.h"

namespace rhythm::transport::detail {
namespace {
constexpr std::array<std::uint8_t, 15> kProtocol{'r', 'h', 'y', 't', 'h', 'm', '-', 'r',
                                                 'o', 'o', 'm', '-', 'v', '0', '1'};
QUIC_BUFFER Alpn() {
    // ConfigurationOpen/ListenerStart synchronously copy this read-only protocol ID.
    return {static_cast<std::uint32_t>(kProtocol.size()),
            const_cast<std::uint8_t*>(kProtocol.data())};
}
}  // namespace
QUIC_ADDR NativeAddress(cluster_auth::Endpoint endpoint) {
    QUIC_ADDR address{};
    if (endpoint.family_ == cluster_auth::AddressFamily::kIpv4) {
        if (std::any_of(endpoint.address_.begin() + 4, endpoint.address_.end(),
                        [](auto value) { return value != 0; }))
            throw std::invalid_argument("transport.address_padding");
        QuicAddrSetFamily(&address, QUIC_ADDRESS_FAMILY_INET);
        std::memcpy(&address.Ipv4.sin_addr, endpoint.address_.data(), 4);
    } else if (endpoint.family_ == cluster_auth::AddressFamily::kIpv6) {
        QuicAddrSetFamily(&address, QUIC_ADDRESS_FAMILY_INET6);
        std::memcpy(&address.Ipv6.sin6_addr, endpoint.address_.data(), 16);
    } else {
        throw std::invalid_argument("transport.address_family");
    }
    QuicAddrSetPort(&address, endpoint.port_);
    return address;
}
cluster_auth::Endpoint ProjectAddress(const QUIC_ADDR& address) {
    cluster_auth::Endpoint endpoint;
    endpoint.port_ = QuicAddrGetPort(&address);
    if (QuicAddrGetFamily(&address) == QUIC_ADDRESS_FAMILY_INET) {
        std::memcpy(endpoint.address_.data(), &address.Ipv4.sin_addr, 4);
    } else if (QuicAddrGetFamily(&address) == QUIC_ADDRESS_FAMILY_INET6) {
        endpoint.family_ = cluster_auth::AddressFamily::kIpv6;
        std::memcpy(endpoint.address_.data(), &address.Ipv6.sin6_addr, 16);
    } else {
        throw std::runtime_error("transport.native_address_family");
    }
    return endpoint;
}
NativeConfiguration::NativeConfiguration(bool client) {
    QUIC_REGISTRATION_CONFIG registration{"rhythm-room", QUIC_EXECUTION_PROFILE_LOW_LATENCY};
    Check(api_->Table().RegistrationOpen(&registration, registration_.Output()));
    QUIC_SETTINGS settings{};
    settings.IdleTimeoutMs = 30000;
    settings.IsSet.IdleTimeoutMs = true;
    settings.HandshakeIdleTimeoutMs = 5000;
    settings.IsSet.HandshakeIdleTimeoutMs = true;
    settings.DatagramReceiveEnabled = true;
    settings.IsSet.DatagramReceiveEnabled = true;
    settings.SendBufferingEnabled = false;
    settings.IsSet.SendBufferingEnabled = true;
    settings.StreamRecvWindowDefault = 262144;
    settings.IsSet.StreamRecvWindowDefault = true;
    settings.ConnFlowControlWindow = 1048576;
    settings.IsSet.ConnFlowControlWindow = true;
    settings.PeerBidiStreamCount = client ? 0 : 1;
    settings.IsSet.PeerBidiStreamCount = true;
    settings.PeerUnidiStreamCount = client ? 1 : 0;
    settings.IsSet.PeerUnidiStreamCount = true;
    auto alpn = Alpn();
    Check(api_->Table().ConfigurationOpen(registration_.Get(), &alpn, 1, &settings,
                                          sizeof(settings), nullptr, configuration_.Output()));
    if (client) {
        QUIC_CREDENTIAL_CONFIG credentials{};
        credentials.Type = QUIC_CREDENTIAL_TYPE_NONE;
        credentials.Flags = QUIC_CREDENTIAL_FLAG_CLIENT |
                            QUIC_CREDENTIAL_FLAG_INDICATE_CERTIFICATE_RECEIVED |
                            QUIC_CREDENTIAL_FLAG_DEFER_CERTIFICATE_VALIDATION |
                            QUIC_CREDENTIAL_FLAG_USE_PORTABLE_CERTIFICATES;
        Check(api_->Table().ConfigurationLoadCredential(configuration_.Get(), &credentials));
    }
}
void NativeConfiguration::LoadHost(const security::HostIdentity& identity) {
    const auto pfx = security::detail::IdentityAccess::Pkcs12(identity);
    QUIC_CERTIFICATE_PKCS12 certificate{pfx.data(), static_cast<std::uint32_t>(pfx.size()), ""};
    QUIC_CREDENTIAL_CONFIG credentials{};
    credentials.Type = QUIC_CREDENTIAL_TYPE_CERTIFICATE_PKCS12;
    credentials.CertificatePkcs12 = &certificate;
    Check(api_->Table().ConfigurationLoadCredential(configuration_.Get(), &credentials));
}
void NativeConfiguration::StartListener(const NativeHandle& listener,
                                        cluster_auth::Endpoint endpoint) {
    const auto address = NativeAddress(endpoint);
    auto alpn = Alpn();
    Check(api_->Table().ListenerStart(listener.Get(), &alpn, 1, &address));
}
void NativeConfiguration::StartConnection(const NativeHandle& connection,
                                          cluster_auth::Endpoint endpoint) {
    if (!endpoint.port_) throw std::invalid_argument("transport.zero_remote_port");
    const auto address = NativeAddress(endpoint);
    Check(api_->Table().SetParam(connection.Get(), QUIC_PARAM_CONN_REMOTE_ADDRESS, sizeof(address),
                                 &address));
    // Explicit remote address avoids DNS; TLS trust comes exclusively from the pin.
    Check(api_->Table().ConnectionStart(connection.Get(), configuration_.Get(),
                                        QuicAddrGetFamily(&address), "rhythm-room",
                                        endpoint.port_));
}
}  // namespace rhythm::transport::detail
