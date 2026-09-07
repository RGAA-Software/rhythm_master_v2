#include <iostream>
#include <stdexcept>
#include <vector>

#include "rhythm/security/identity.h"

int main() {
    using namespace rhythm::security;
    try {
        const auto check = [](bool value) {
            if (!value) throw std::runtime_error("identity.contract");
        };
        auto first = HostIdentity::Create();
        auto second = HostIdentity::Create();
        check(first.Fingerprint() != second.Fingerprint());
        check(VerifyCertificate(first.CertificateDer(), first.Fingerprint()));
        check(!VerifyCertificate(first.CertificateDer(), second.Fingerprint()));
        auto invalid = std::vector(first.CertificateDer().begin(), first.CertificateDer().end());
        invalid.push_back(0);
        check(!VerifyCertificate(invalid, first.Fingerprint()));
        invalid = {1, 2, 3};
        check(!VerifyCertificate(invalid, first.Fingerprint()));
        check(!VerifyCertificate({}, first.Fingerprint()));
        const auto token = GenerateToken();
        check(TokensEqual(token, token) && !TokensEqual(token, GenerateToken()));
        auto changed = token;
        changed.front() ^= 1;
        check(!TokensEqual(token, changed));
        changed = token;
        changed.back() ^= 1;
        check(!TokensEqual(token, changed));
        const auto pin = first.Fingerprint();
        auto moved = std::move(first);
        check(moved.Fingerprint() == pin && VerifyCertificate(moved.CertificateDer(), pin));
        std::cout << "identity contracts passed: independent P-256 identities, certificate pins, "
                     "bounded DER, CSPRNG and token comparison\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
