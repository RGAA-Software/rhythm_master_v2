// OpenSSL fixture construction stays inside this isolated native-adapter test.
#include <openssl/evp.h>
#include <openssl/x509v3.h>

#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

#include "rhythm/security/identity.h"

namespace {
void Check(bool value) {
    if (!value) throw std::runtime_error("certificate.policy_contract");
}
template <typename T, auto Free>
struct Closer {
    void operator()(T* value) const { Free(value); }
};
using Certificate = std::unique_ptr<X509, Closer<X509, X509_free>>;
using Key = std::unique_ptr<EVP_PKEY, Closer<EVP_PKEY, EVP_PKEY_free>>;
using Extension = std::unique_ptr<X509_EXTENSION, Closer<X509_EXTENSION, X509_EXTENSION_free>>;
bool VerifyVariant(long before, long after, bool server_purpose) {
    using namespace rhythm::security;
    auto identity = HostIdentity::Create();
    const auto der = identity.CertificateDer();
    const auto* cursor = der.data();
    Certificate certificate(d2i_X509(nullptr, &cursor, static_cast<long>(der.size())));
    Key key(EVP_PKEY_Q_keygen(nullptr, nullptr, "EC", "prime256v1"));
    Check(certificate && key);
    Check(X509_gmtime_adj(X509_getm_notBefore(certificate.get()), before) != nullptr);
    Check(X509_gmtime_adj(X509_getm_notAfter(certificate.get()), after) != nullptr);
    if (!server_purpose) {
        const auto index = X509_get_ext_by_NID(certificate.get(), NID_ext_key_usage, -1);
        Check(index >= 0);
        Extension removed(X509_delete_ext(certificate.get(), index));
        Extension client(X509V3_EXT_conf_nid(nullptr, nullptr, NID_ext_key_usage, "clientAuth"));
        Check(removed && client && X509_add_ext(certificate.get(), client.get(), -1) == 1);
    }
    Check(X509_set_pubkey(certificate.get(), key.get()) == 1);
    Check(X509_sign(certificate.get(), key.get(), EVP_sha256()) > 0);
    CertificatePin pin{};
    unsigned int digest_size = 0;
    Check(X509_digest(certificate.get(), EVP_sha256(), pin.data(), &digest_size) == 1 &&
          digest_size == pin.size());
    const auto size = i2d_X509(certificate.get(), nullptr);
    Check(size > 0 && size <= 8192);
    std::vector<std::uint8_t> serialized(static_cast<std::size_t>(size));
    auto output = serialized.data();
    Check(i2d_X509(certificate.get(), &output) == size);
    return VerifyCertificate(serialized, pin);
}
}  // namespace
int main() {
    try {
        Check(VerifyVariant(-60, 3600, true));
        Check(!VerifyVariant(-3600, -60, true));
        Check(!VerifyVariant(60, 3600, true));
        Check(!VerifyVariant(-60, 3600, false));
        std::cout << "certificate policy passed: matching pins still reject expired, future "
                     "and client-only certificates\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
