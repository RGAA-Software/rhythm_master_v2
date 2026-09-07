#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>

namespace rhythm::security {
using CertificatePin = std::array<std::uint8_t, 32>;
using Token = std::array<std::uint8_t, 32>;
namespace detail {
class IdentityAccess;
}
// Backend CSPRNG and constant-time comparison; tokens must not be logged.
Token GenerateToken();
bool TokensEqual(const Token& first, const Token& second);
// Pin mode replaces CA/name trust with the invitation's exact certificate hash,
// while still checking certificate dates and server purpose. Call from the TLS
// certificate callback before accepting the connection; TLS proves key possession.
bool VerifyCertificate(std::span<const std::uint8_t> der, const CertificatePin& expected);
class HostIdentity final {
   public:
    static HostIdentity Create();
    // Windows current-user DPAPI vault; caller supplies an existing private host
    // directory. Other hosts fail explicitly until a native secret store exists.
    // Load failure never regenerates or replaces an existing identity. Certificates
    // currently expire after 30 days; rotation requires fresh invitation pins.
    static HostIdentity LoadProtected(const std::filesystem::path& path);
    void SaveProtected(const std::filesystem::path& path) const;
    ~HostIdentity();
    HostIdentity(HostIdentity&&) noexcept;
    HostIdentity& operator=(HostIdentity&&) noexcept;
    HostIdentity(const HostIdentity&) = delete;
    HostIdentity& operator=(const HostIdentity&) = delete;
    CertificatePin Fingerprint() const;
    std::span<const std::uint8_t> CertificateDer() const;

   private:
    friend class detail::IdentityAccess;
    class Impl;
    explicit HostIdentity(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_{};
};
}  // namespace rhythm::security
