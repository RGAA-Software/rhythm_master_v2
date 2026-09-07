#include "rhythm/security/identity.h"

#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/pkcs12.h>
#include <openssl/rand.h>
#include <openssl/x509v3.h>

#include <algorithm>
#include <stdexcept>

#include "identity_access.h"
#include "secret_bytes.h"
#include "vault_file.h"

namespace rhythm::security {
namespace {
void Require(bool condition) {
    if (!condition) throw std::runtime_error("security.crypto_failed");
}
// OpenSSL allocations transfer immediately to exclusive ownership at this C API boundary.
template <typename T, auto Free>
struct Closer {
    void operator()(T* value) const { Free(value); }
};
using Key = std::unique_ptr<EVP_PKEY, Closer<EVP_PKEY, EVP_PKEY_free>>;
using Certificate = std::unique_ptr<X509, Closer<X509, X509_free>>;
using Pkcs12 = std::unique_ptr<PKCS12, Closer<PKCS12, PKCS12_free>>;
using BigNumber = std::unique_ptr<BIGNUM, Closer<BIGNUM, BN_free>>;
using Integer = std::unique_ptr<ASN1_INTEGER, Closer<ASN1_INTEGER, ASN1_INTEGER_free>>;
using Extension = std::unique_ptr<X509_EXTENSION, Closer<X509_EXTENSION, X509_EXTENSION_free>>;
CertificatePin Pin(const Certificate& certificate) {
    CertificatePin pin{};
    unsigned int length = 0;
    Require(X509_digest(certificate.get(), EVP_sha256(), pin.data(), &length) == 1 &&
            length == pin.size());
    return pin;
}
std::vector<std::uint8_t> Der(const Certificate& certificate) {
    const auto size = i2d_X509(certificate.get(), nullptr);
    Require(size > 0 && size <= 8192);
    std::vector<std::uint8_t> result(static_cast<std::size_t>(size));
    // Borrowed serialization cursor is confined to this synchronous C API call.
    auto cursor = result.data();
    Require(i2d_X509(certificate.get(), &cursor) == size);
    return result;
}
bool CurrentServerCertificate(const Certificate& certificate) {
    return X509_cmp_current_time(X509_get0_notBefore(certificate.get())) < 0 &&
           X509_cmp_current_time(X509_get0_notAfter(certificate.get())) > 0 &&
           X509_check_purpose(certificate.get(), X509_PURPOSE_SSL_SERVER, 0) == 1;
}
void AddExtension(Certificate& certificate, int id, const char* text) {
    Extension extension(X509V3_EXT_conf_nid(nullptr, nullptr, id, text));
    Require(extension && X509_add_ext(certificate.get(), extension.get(), -1) == 1);
}
}  // namespace
class HostIdentity::Impl final {
   public:
    detail::SecretBytes pfx_{};
    std::vector<std::uint8_t> der_{};
    CertificatePin pin_{};
};
HostIdentity::HostIdentity(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
HostIdentity::~HostIdentity() = default;
HostIdentity::HostIdentity(HostIdentity&&) noexcept = default;
HostIdentity& HostIdentity::operator=(HostIdentity&&) noexcept = default;
Token GenerateToken() {
    Token token{};
    Require(RAND_priv_bytes(token.data(), static_cast<int>(token.size())) == 1);
    return token;
}
bool TokensEqual(const Token& first, const Token& second) {
    return CRYPTO_memcmp(first.data(), second.data(), first.size()) == 0;
}
HostIdentity HostIdentity::Create() {
    Key key(EVP_PKEY_Q_keygen(nullptr, nullptr, "EC", "prime256v1"));
    Certificate certificate(X509_new());
    Require(key && certificate && X509_set_version(certificate.get(), 2) == 1);
    auto random_serial = GenerateToken();
    random_serial[0] = static_cast<std::uint8_t>((random_serial[0] & 0x7f) | 1);
    BigNumber number(BN_bin2bn(random_serial.data(), 16, nullptr));
    Require(bool(number));
    Integer serial(BN_to_ASN1_INTEGER(number.get(), nullptr));
    Require(serial && X509_set_serialNumber(certificate.get(), serial.get()) == 1);
    Require(X509_gmtime_adj(X509_getm_notBefore(certificate.get()), -60) != nullptr);
    Require(X509_gmtime_adj(X509_getm_notAfter(certificate.get()), 30 * 24 * 60 * 60) != nullptr);
    Require(X509_set_pubkey(certificate.get(), key.get()) == 1);
    constexpr unsigned char kName[] = "Rhythm Master Host";
    Require(X509_NAME_add_entry_by_txt(X509_get_subject_name(certificate.get()), "CN", MBSTRING_ASC,
                                       kName, -1, -1, 0) == 1);
    Require(X509_set_issuer_name(certificate.get(), X509_get_subject_name(certificate.get())) == 1);
    AddExtension(certificate, NID_basic_constraints, "critical,CA:FALSE");
    AddExtension(certificate, NID_key_usage, "critical,digitalSignature");
    AddExtension(certificate, NID_ext_key_usage, "serverAuth");
    Require(X509_sign(certificate.get(), key.get(), EVP_sha256()) > 0);
    Require(X509_verify(certificate.get(), key.get()) == 1 &&
            CurrentServerCertificate(certificate));
    Pkcs12 bundle(PKCS12_create("", "Rhythm Master Host", key.get(), certificate.get(), nullptr, 0,
                                0, PKCS12_DEFAULT_ITER, PKCS12_DEFAULT_ITER, 0));
    Require(bool(bundle));
    const auto size = i2d_PKCS12(bundle.get(), nullptr);
    Require(size > 0 && size <= 65536);
    auto impl = std::make_unique<Impl>();
    impl->pfx_ = detail::SecretBytes(static_cast<std::size_t>(size));
    auto cursor = impl->pfx_.MutableBytes().data();
    Require(i2d_PKCS12(bundle.get(), &cursor) == size);
    impl->pin_ = Pin(certificate);
    impl->der_ = Der(certificate);
    return HostIdentity(std::move(impl));
}
HostIdentity HostIdentity::LoadProtected(const std::filesystem::path& path) {
    auto secret = detail::ReadVault(path);
    const auto bytes = secret.Bytes();
    const auto* cursor = bytes.data();
    Pkcs12 bundle(d2i_PKCS12(nullptr, &cursor, static_cast<long>(bytes.size())));
    Require(bundle && cursor == bytes.data() + bytes.size());
    // OpenSSL output parameters transfer ownership immediately, including failure paths.
    EVP_PKEY* native_key = nullptr;
    X509* native_certificate = nullptr;
    const auto parsed = PKCS12_parse(bundle.get(), "", &native_key, &native_certificate, nullptr);
    Key key(native_key);
    Certificate certificate(native_certificate);
    Require(parsed == 1 && key && certificate);
    char group[80]{};
    std::size_t group_size = 0;
    Require(EVP_PKEY_is_a(key.get(), "EC") == 1 &&
            EVP_PKEY_get_group_name(key.get(), group, sizeof(group), &group_size) == 1 &&
            std::string_view(group) == "prime256v1");
    Require(X509_check_private_key(certificate.get(), key.get()) == 1 &&
            X509_verify(certificate.get(), key.get()) == 1 &&
            CurrentServerCertificate(certificate));
    auto impl = std::make_unique<Impl>();
    impl->pfx_ = std::move(secret);
    impl->der_ = Der(certificate);
    impl->pin_ = Pin(certificate);
    return HostIdentity(std::move(impl));
}
void HostIdentity::SaveProtected(const std::filesystem::path& path) const {
    if (!impl_) throw std::logic_error("security.empty_identity");
    detail::WriteVault(path, impl_->pfx_.Bytes());
}
bool VerifyCertificate(std::span<const std::uint8_t> der, const CertificatePin& expected) {
    if (der.empty() || der.size() > 8192) return false;
    try {
        const auto* cursor = der.data();
        Certificate certificate(d2i_X509(nullptr, &cursor, static_cast<long>(der.size())));
        if (!certificate || cursor != der.data() + der.size() ||
            !CurrentServerCertificate(certificate))
            return false;
        const auto actual = Pin(certificate);
        return CRYPTO_memcmp(actual.data(), expected.data(), actual.size()) == 0;
    } catch (const std::exception&) {
        return false;
    }
}
CertificatePin HostIdentity::Fingerprint() const {
    if (!impl_) throw std::logic_error("security.empty_identity");
    return impl_->pin_;
}
std::span<const std::uint8_t> HostIdentity::CertificateDer() const {
    if (!impl_) throw std::logic_error("security.empty_identity");
    return impl_->der_;
}
std::span<const std::uint8_t> detail::IdentityAccess::Pkcs12(const HostIdentity& identity) {
    if (!identity.impl_) throw std::logic_error("security.empty_identity");
    return identity.impl_->pfx_.Bytes();
}
}  // namespace rhythm::security
