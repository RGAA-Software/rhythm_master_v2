#include <stdexcept>

#include "secret_bytes.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
// Windows declarations must precede the cryptography API headers.
#include <dpapi.h>
#include <wincrypt.h>
#endif

namespace rhythm::security::detail {
#ifdef _WIN32
namespace {
// DPAPI allocates this boundary blob with LocalAlloc. It is wiped and freed even
// if validation or copying throws; it never escapes this synchronous adapter.
class LocalBlob final {
   public:
    ~LocalBlob() {
        if (blob_.pbData) {
            SecureZeroMemory(blob_.pbData, blob_.cbData);
            LocalFree(blob_.pbData);
        }
    }
    LocalBlob() = default;
    LocalBlob(const LocalBlob&) = delete;
    LocalBlob& operator=(const LocalBlob&) = delete;
    DATA_BLOB& Native() { return blob_; }
    std::span<const std::uint8_t> Bytes() const { return {blob_.pbData, blob_.cbData}; }

   private:
    DATA_BLOB blob_{0, nullptr};
};
constexpr std::uint8_t kContext[] = "RhythmMaster/HostIdentity/v1";
void Transform(std::span<const std::uint8_t> bytes, bool encrypt, LocalBlob& output) {
    if (bytes.empty() || bytes.size() > (encrypt ? 65536u : 131072u))
        throw std::runtime_error("security.secret_size");
    // DPAPI's input descriptors are non-const, but the API does not modify input.
    DATA_BLOB input{static_cast<DWORD>(bytes.size()), const_cast<BYTE*>(bytes.data())};
    DATA_BLOB entropy{sizeof(kContext), const_cast<BYTE*>(kContext)};
    const auto success =
            encrypt ? CryptProtectData(&input, L"Rhythm Master host identity", &entropy, nullptr,
                                       nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output.Native())
                    : CryptUnprotectData(&input, nullptr, &entropy, nullptr, nullptr,
                                         CRYPTPROTECT_UI_FORBIDDEN, &output.Native());
    if (!success) throw std::runtime_error("security.protection_failed");
}
}  // namespace
std::vector<std::uint8_t> ProtectSecret(std::span<const std::uint8_t> bytes) {
    LocalBlob result;
    Transform(bytes, true, result);
    const auto protected_bytes = result.Bytes();
    if (protected_bytes.empty() || protected_bytes.size() > 131072)
        throw std::runtime_error("security.secret_size");
    return {protected_bytes.begin(), protected_bytes.end()};
}
SecretBytes UnprotectSecret(std::span<const std::uint8_t> bytes) {
    LocalBlob result;
    Transform(bytes, false, result);
    return SecretBytes(result.Bytes());
}
#else
std::vector<std::uint8_t> ProtectSecret(std::span<const std::uint8_t>) {
    throw std::runtime_error("security.host_vault_unsupported");
}
SecretBytes UnprotectSecret(std::span<const std::uint8_t>) {
    throw std::runtime_error("security.host_vault_unsupported");
}
#endif
}  // namespace rhythm::security::detail
