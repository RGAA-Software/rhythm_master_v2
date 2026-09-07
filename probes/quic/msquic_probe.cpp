// Isolated native API adapter for candidate verification, not a runtime contract.
#include <msquic.h>
#if defined(_WIN32) && !defined(RHYTHM_QUIC_OPENSSL)
#include <wincrypt.h>
#endif

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "connection.h"
#include "flow_control.h"
#include "stress.h"
#ifdef RHYTHM_IDENTITY_PROBE
#include "identity_access.h"
#include "identity_fixture.h"
#include "rhythm/security/identity.h"
#include "room_wire.h"
#endif

namespace {
using rhythm::quic_probe::Api;
using rhythm::quic_probe::Check;
using rhythm::quic_probe::Connection;
using rhythm::quic_probe::Handle;
using rhythm::quic_probe::Kind;
using rhythm::quic_probe::Require;
class Listener final {
   public:
    Listener(std::shared_ptr<Api> api, std::shared_ptr<Connection> server, HQUIC configuration)
        : api_(std::move(api)), server_(std::move(server)), configuration_(configuration) {}
    static QUIC_STATUS QUIC_API Callback(HQUIC, void* context, QUIC_LISTENER_EVENT* event) {
        if (!context || !event) return QUIC_STATUS_INVALID_PARAMETER;
        auto& self = *static_cast<Listener*>(context);
        if (event->Type != QUIC_LISTENER_EVENT_NEW_CONNECTION) return QUIC_STATUS_SUCCESS;
        std::lock_guard lock(self.server_->mutex_);
        if (self.server_->connection_.Get()) return QUIC_STATUS_CONNECTION_REFUSED;
        const auto connection = event->NEW_CONNECTION.Connection;
        self.api_->Table().SetCallbackHandler(
                connection, reinterpret_cast<void*>(Connection::Callback), self.server_.get());
        const auto status =
                self.api_->Table().ConnectionSetConfiguration(connection, self.configuration_);
        if (QUIC_FAILED(status)) return status;  // Library retains ownership on rejection.
        self.server_->connection_.Adopt(connection);
        return QUIC_STATUS_SUCCESS;
    }
    std::shared_ptr<Api> api_{};
    std::shared_ptr<Connection> server_{};
    // Configuration outlives this listener in the probe scope; native-only borrow.
    HQUIC configuration_ = nullptr;
    Handle listener_{api_, Kind::kListener};
};
std::vector<std::uint8_t> Read(const std::filesystem::path& path) {
    Require(std::filesystem::file_size(path) <= 65536, "fixture.size");
    std::ifstream file(path, std::ios::binary);
    Require(file.is_open(), "fixture.open");
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
#if defined(_WIN32) && !defined(RHYTHM_QUIC_OPENSSL)
class TestCertificate final {
   public:
    explicit TestCertificate(std::span<const std::uint8_t> pfx) {
        CRYPT_DATA_BLOB blob{static_cast<DWORD>(pfx.size()), const_cast<BYTE*>(pfx.data())};
        store_ = PFXImportCertStore(&blob, L"probe-only", PKCS12_NO_PERSIST_KEY);
        Require(store_ != nullptr, "certificate.memory_store:" + std::to_string(GetLastError()));
        certificate_.reset(CertEnumCertificatesInStore(store_, nullptr));
        // No throw after acquiring the store: destructor must own every success path.
    }
    ~TestCertificate() {
        certificate_.reset();
        if (store_) CertCloseStore(store_, 0);
    }
    TestCertificate(const TestCertificate&) = delete;
    TestCertificate& operator=(const TestCertificate&) = delete;
    void Load(const Api& api, const Handle& configuration) const {
        Require(certificate_ != nullptr, "certificate.missing");
        QUIC_CREDENTIAL_CONFIG credentials{};
        credentials.Type = QUIC_CREDENTIAL_TYPE_CERTIFICATE_CONTEXT;
        credentials.CertificateContext = const_cast<CERT_CONTEXT*>(certificate_.get());
        Check(api.Table().ConfigurationLoadCredential(configuration.Get(), &credentials),
              "server.credentials");
    }

   private:
    struct CertificateCloser {
        void operator()(PCCERT_CONTEXT certificate) const {
            CertFreeCertificateContext(certificate);
        }
    };
    // Ephemeral native store and key; never imports a root into the machine/user trust stores.
    HCERTSTORE store_ = nullptr;
    std::unique_ptr<const CERT_CONTEXT, CertificateCloser> certificate_{};
};
#endif
enum class Role {
    kLoopback,
    kServer,
    kClient,
    kFlow,
    kCancelFlow,
    kAdmission,
    kBadInvitation,
    kRevokedInvitation
};
enum class CancelAt { kNever, kHandshake, kSend };
void Scenario(const std::filesystem::path& fixtures, bool correct_certificate,
              Role role = Role::kLoopback, std::string host = "127.0.0.1", std::uint16_t port = 0,
              CancelAt cancel_at = CancelAt::kNever, std::size_t stress_peers = 0,
              std::span<const std::uint8_t> generated_pfx = {},
              std::function<bool(std::span<const std::uint8_t>)> verify_certificate = {}) {
    auto api = std::make_shared<Api>();
    Handle registration(api, Kind::kRegistration);
    QUIC_REGISTRATION_CONFIG registration_config{"rhythm-quic-probe",
                                                 QUIC_EXECUTION_PROFILE_LOW_LATENCY};
    Check(api->Table().RegistrationOpen(&registration_config, registration.Output()),
          "registration");
    const std::array<std::uint8_t, 15> protocol{'r', 'h', 'y', 't', 'h', 'm', '-', 'p',
                                                'r', 'o', 'b', 'e', '-', 'v', '1'};
    QUIC_BUFFER alpn{static_cast<std::uint32_t>(protocol.size()),
                     const_cast<std::uint8_t*>(protocol.data())};
    QUIC_SETTINGS settings{};
    settings.IdleTimeoutMs = 5000;
    settings.IsSet.IdleTimeoutMs = true;
    settings.HandshakeIdleTimeoutMs = 3000;
    settings.IsSet.HandshakeIdleTimeoutMs = true;
    settings.DatagramReceiveEnabled = true;
    settings.IsSet.DatagramReceiveEnabled = true;
    settings.PeerUnidiStreamCount = 1;
    settings.IsSet.PeerUnidiStreamCount = true;
    settings.PeerBidiStreamCount = 1;
    settings.IsSet.PeerBidiStreamCount = true;
    if (role == Role::kFlow || role == Role::kCancelFlow) {
        settings.SendBufferingEnabled = false;
        settings.IsSet.SendBufferingEnabled = true;
    }
    Handle server_config(api, Kind::kConfiguration);
    Handle client_config(api, Kind::kConfiguration);
    Check(api->Table().ConfigurationOpen(registration.Get(), &alpn, 1, &settings, sizeof(settings),
                                         nullptr, server_config.Output()),
          "server.configuration");
    Check(api->Table().ConfigurationOpen(registration.Get(), &alpn, 1, &settings, sizeof(settings),
                                         nullptr, client_config.Output()),
          "client.configuration");
    const auto fixture_pfx =
            generated_pfx.empty() ? Read(fixtures / "server.pfx") : std::vector<std::uint8_t>{};
    const auto pfx = generated_pfx.empty() ? std::span(fixture_pfx) : generated_pfx;
#if defined(_WIN32) && !defined(RHYTHM_QUIC_OPENSSL)
    TestCertificate(pfx).Load(*api, server_config);
#else
    QUIC_CERTIFICATE_PKCS12 certificate{pfx.data(), static_cast<std::uint32_t>(pfx.size()),
                                        generated_pfx.empty() ? "probe-only" : ""};
    QUIC_CREDENTIAL_CONFIG server_credentials{};
    server_credentials.Type = QUIC_CREDENTIAL_TYPE_CERTIFICATE_PKCS12;
    server_credentials.CertificatePkcs12 = &certificate;
    Check(api->Table().ConfigurationLoadCredential(server_config.Get(), &server_credentials),
          "server.credentials");
#endif
    QUIC_CREDENTIAL_CONFIG client_credentials{};
    client_credentials.Type = QUIC_CREDENTIAL_TYPE_NONE;
    client_credentials.Flags = QUIC_CREDENTIAL_FLAG_CLIENT |
                               QUIC_CREDENTIAL_FLAG_INDICATE_CERTIFICATE_RECEIVED |
                               QUIC_CREDENTIAL_FLAG_DEFER_CERTIFICATE_VALIDATION |
                               QUIC_CREDENTIAL_FLAG_USE_PORTABLE_CERTIFICATES;
    Check(api->Table().ConfigurationLoadCredential(client_config.Get(), &client_credentials),
          "client.credentials");
    if (stress_peers) {
        rhythm::quic_probe::StressConnections(api, registration, server_config, client_config,
                                              Read(fixtures / "server.der"), alpn, stress_peers);
        return;
    }
    auto server = std::make_shared<Connection>(api);
    auto client = std::make_shared<Connection>(api);
    server->bounded_receive_ = role == Role::kFlow || role == Role::kCancelFlow;
    const auto admission = role == Role::kAdmission || role == Role::kBadInvitation ||
                           role == Role::kRevokedInvitation;
    if (admission) {
        client->bounded_receive_ = true;
        server->bounded_receive_ = true;
    }
    client->verify_certificate_ = std::move(verify_certificate);
    if (!client->verify_certificate_) {
        client->expected_certificate_ = Read(fixtures / "server.der");
        Require(!client->expected_certificate_.empty(), "fixture.empty_certificate");
        if (!correct_certificate) client->expected_certificate_.front() ^= 1;
    }
    // Declared last: closes the accept callback before connection state is released.
    Listener listener(api, server, server_config.Get());
    QUIC_ADDR address{};
    Require(QuicAddrFromString(host.c_str(), port, &address), "endpoint.address");
    if (role != Role::kClient) {
        Check(api->Table().ListenerOpen(registration.Get(), Listener::Callback, &listener,
                                        listener.listener_.Output()),
              "listener.open");
        Check(api->Table().ListenerStart(listener.listener_.Get(), &alpn, 1, &address),
              "listener.start");
        std::uint32_t address_length = sizeof(address);
        Check(api->Table().GetParam(listener.listener_.Get(), QUIC_PARAM_LISTENER_LOCAL_ADDRESS,
                                    &address_length, &address),
              "listener.port");
    }
    if (role == Role::kServer) {
        std::cout << "QUIC server ready" << std::endl;
        Require(server->Wait([&] {
            return server->connected_ && server->datagram_enabled_ &&
                   server->stream_bytes_.size() == server->payload_.size() &&
                   server->datagram_bytes_.size() == server->payload_.size();
        }),
                "remote.receive_timeout");
        {
            std::lock_guard lock(server->mutex_);
            Require(std::equal(server->stream_bytes_.begin(), server->stream_bytes_.end(),
                               server->payload_.begin()) &&
                            std::equal(server->datagram_bytes_.begin(),
                                       server->datagram_bytes_.end(), server->payload_.begin()),
                    "remote.receive_bytes");
        }
        server->Send(true);
        Require(server->Wait([&] { return server->closed_; }), "remote.shutdown_timeout");
        server->DrainSends();
        std::cout << "Remote reliable stream, DATAGRAM/reply and shutdown passed\n";
        return;
    }
    Check(api->Table().ConnectionOpen(registration.Get(), Connection::Callback, client.get(),
                                      client->connection_.Output()),
          "client.open");
    Check(api->Table().ConnectionStart(client->connection_.Get(), client_config.Get(),
                                       QUIC_ADDRESS_FAMILY_INET, host.c_str(),
                                       QuicAddrGetPort(&address)),
          "client.start");
    if (cancel_at == CancelAt::kHandshake) {
        api->Table().ConnectionShutdown(client->connection_.Get(),
                                        QUIC_CONNECTION_SHUTDOWN_FLAG_NONE, 0);
        Require(client->Wait([&] { return client->closed_; }), "cancel.handshake_timeout");
        client->DrainSends();
        return;
    }
    Require(client->Wait([&] { return client->connected_ || client->closed_; }),
            "handshake.timeout");
    if (!correct_certificate) {
        std::lock_guard lock(client->mutex_);
        Require(client->closed_ && !client->connected_ && client->certificate_seen_ &&
                        !client->certificate_accepted_,
                "certificate.rejection");
        std::cout << "Incorrect pinned certificate rejected before connected\n";
        return;
    }
    const auto capabilities = client->Wait([&] {
        return client->connected_ && client->certificate_accepted_ && client->datagram_enabled_;
    });
    Require(capabilities, "handshake.capabilities" + client->State());
#ifdef RHYTHM_IDENTITY_PROBE
    if (admission) {
        rhythm::security::probe::RoomWire(*client, *server, role == Role::kBadInvitation,
                                          role == Role::kRevokedInvitation);
        return;
    }
#endif
    if (role == Role::kFlow || role == Role::kCancelFlow) {
        rhythm::quic_probe::FlowControl(*client, *server, role == Role::kCancelFlow);
        return;
    }
    client->Send(true);
    Check(api->Table().StreamOpen(client->connection_.Get(), QUIC_STREAM_OPEN_FLAG_UNIDIRECTIONAL,
                                  Connection::StreamCallback, client.get(),
                                  client->stream_.Output()),
          "stream.open");
    Check(api->Table().StreamStart(client->stream_.Get(), QUIC_STREAM_START_FLAG_IMMEDIATE),
          "stream.start");
    client->Send(false);
    if (cancel_at == CancelAt::kSend) {
        api->Table().ConnectionShutdown(client->connection_.Get(),
                                        QUIC_CONNECTION_SHUTDOWN_FLAG_NONE, 0);
        Require(client->Wait([&] { return client->closed_; }), "cancel.send_timeout");
        client->DrainSends();
        return;
    }
    if (role == Role::kClient) {
        Require(client->Wait(
                        [&] { return client->datagram_bytes_.size() == client->payload_.size(); }),
                "remote.reply_timeout");
        std::lock_guard lock(client->mutex_);
        Require(std::equal(client->datagram_bytes_.begin(), client->datagram_bytes_.end(),
                           client->payload_.begin()),
                "remote.reply_bytes");
    } else {
        Require(server->Wait([&] {
            return server->stream_bytes_.size() == client->payload_.size() &&
                   server->datagram_bytes_.size() == client->payload_.size();
        }),
                "receive.timeout");
        {
            std::lock_guard lock(server->mutex_);
            Require(std::equal(server->stream_bytes_.begin(), server->stream_bytes_.end(),
                               client->payload_.begin()) &&
                            std::equal(server->datagram_bytes_.begin(),
                                       server->datagram_bytes_.end(), client->payload_.begin()),
                    "receive.bytes");
        }
    }
    api->Table().ConnectionShutdown(client->connection_.Get(), QUIC_CONNECTION_SHUTDOWN_FLAG_NONE,
                                    0);
    Require(client->Wait([&] { return client->closed_; }), "shutdown.timeout");
    client->DrainSends();
    std::cout << "Pinned TLS handshake, reliable stream, DATAGRAM and shutdown passed\n";
}
}  // namespace
int main(int argc, char* argv[]) {
    try {
#ifdef RHYTHM_IDENTITY_PROBE
        using namespace rhythm::security;
        Require(argc == 1 || argc == 2, "usage: identity_quic_tests [protected_identity]");
        auto identity =
                argc == 2 ? HostIdentity::LoadProtected(argv[1]) : probe::CreateTransportIdentity();
        const auto pfx = detail::IdentityAccess::Pkcs12(identity);
        for (bool correct : {true, false, true}) {
            auto pin = identity.Fingerprint();
            if (!correct) pin.front() ^= 1;
            Scenario({}, correct, Role::kLoopback, "127.0.0.1", 0, CancelAt::kNever, 0, pfx,
                     [pin](auto der) { return VerifyCertificate(der, pin); });
        }
        for (auto cancel_at : {CancelAt::kHandshake, CancelAt::kSend}) {
            Scenario({}, true, Role::kLoopback, "127.0.0.1", 0, cancel_at, 0, pfx,
                     [pin = identity.Fingerprint()](auto der) {
                         return VerifyCertificate(der, pin);
                     });
        }
        for (auto role : {Role::kAdmission, Role::kBadInvitation, Role::kRevokedInvitation}) {
            Scenario({}, true, role, "127.0.0.1", 0, CancelAt::kNever, 0, pfx,
                     [pin = identity.Fingerprint()](auto der) {
                         return VerifyCertificate(der, pin);
                     });
        }
        std::cout << "Generated identity, SHA-256 pin and QUIC cancellation passed\n";
        return 0;
#else
        Require(argc == 2 || argc == 3 || argc == 4 || argc == 5,
                "usage: msquic_probe <fixtures> [server|client <IPv4> <port>] or <fixtures> stress "
                "<count>");
        if (argc == 3) {
            Require(std::string_view(argv[2]) == "flow", "flow.mode");
            Scenario(argv[1], true, Role::kFlow);
            Scenario(argv[1], true, Role::kCancelFlow);
            return 0;
        }
        if (argc == 4) {
            Require(std::string_view(argv[2]) == "stress", "stress.mode");
            const std::string_view text(argv[3]);
            std::size_t count = 0;
            const auto parsed = std::from_chars(text.data(), text.data() + text.size(), count);
            Require(parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() &&
                            count > 0 && count <= 1000,
                    "stress.count");
            Scenario(argv[1], true, Role::kLoopback, "127.0.0.1", 0, CancelAt::kNever, count);
            return 0;
        }
        if (argc == 5) {
            const std::string mode(argv[2]);
            Require(mode == "server" || mode == "client", "endpoint.mode");
            const std::string text(argv[4]);
            std::uint16_t port = 0;
            const auto parsed = std::from_chars(text.data(), text.data() + text.size(), port);
            Require(parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() && port > 0,
                    "endpoint.port");
            Scenario(argv[1], true, mode == "server" ? Role::kServer : Role::kClient, argv[3],
                     port);
            return 0;
        }
        Scenario(argv[1], true);
        Scenario(argv[1], false);
        Scenario(argv[1], true);
        for (int cycle = 0; cycle < 8; ++cycle) {
            Scenario(argv[1], true, Role::kLoopback, "127.0.0.1", 0, CancelAt::kHandshake);
            Scenario(argv[1], true, Role::kLoopback, "127.0.0.1", 0, CancelAt::kSend);
        }
        std::cout << "16 handshake/send cancellation races drained and closed\n";
#endif
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
