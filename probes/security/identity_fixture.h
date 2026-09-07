#pragma once

#include <filesystem>
#include <stdexcept>

#include "rhythm/security/identity.h"

namespace rhythm::security::probe {
// Test-owned encrypted vault only; no certificate enters an OS trust store.
class TemporaryVault final {
   public:
    TemporaryVault() {
        constexpr char kHex[] = "0123456789abcdef";
        std::string name = "rhythm-tls-";
        const auto token = GenerateToken();
        for (std::size_t index = 0; index < 16; ++index) {
            name += kHex[token[index] >> 4];
            name += kHex[token[index] & 15];
        }
        directory_ = std::filesystem::temp_directory_path() / name;
        if (!std::filesystem::create_directory(directory_))
            throw std::runtime_error("identity.fixture_directory");
    }
    ~TemporaryVault() {
        std::error_code ignored;
        std::filesystem::remove(Path(), ignored);
        std::filesystem::remove(directory_ / ".writer", ignored);
        std::filesystem::remove(directory_, ignored);
    }
    TemporaryVault(const TemporaryVault&) = delete;
    TemporaryVault& operator=(const TemporaryVault&) = delete;
    std::filesystem::path Path() const { return directory_ / "host.identity"; }

   private:
    std::filesystem::path directory_{};
};
inline HostIdentity CreateTransportIdentity() {
    auto identity = HostIdentity::Create();
#ifdef _WIN32
    TemporaryVault vault;
    identity.SaveProtected(vault.Path());
    auto restored = HostIdentity::LoadProtected(vault.Path());
    if (identity.Fingerprint() != restored.Fingerprint())
        throw std::runtime_error("identity.fixture_reload");
    return restored;
#else
    return identity;
#endif
}
}  // namespace rhythm::security::probe
