#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "identity_access.h"
#include "rhythm/security/identity.h"
#include "rhythm/storage/atomic_file.h"

namespace {
void Check(bool value) {
    if (!value) throw std::runtime_error("vault.contract");
}
class TestDirectory final {
   public:
    TestDirectory() {
        const auto token = rhythm::security::GenerateToken();
        std::string name = "rhythm-identity-test-";
        constexpr char kHex[] = "0123456789abcdef";
        for (std::size_t index = 0; index < 16; ++index) {
            name += kHex[token[index] >> 4];
            name += kHex[token[index] & 15];
        }
        path_ = std::filesystem::temp_directory_path() / name;
        Check(std::filesystem::create_directory(path_));
    }
    ~TestDirectory() {
        std::error_code ignored;
        std::filesystem::remove(path_ / "host.identity", ignored);
        std::filesystem::remove(path_ / ".writer", ignored);
        std::filesystem::remove(path_, ignored);
    }
    const std::filesystem::path& Path() const { return path_; }

   private:
    std::filesystem::path path_{};
};
#ifdef _WIN32
std::vector<char> Read(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), {}};
}
void Write(const std::filesystem::path& path, const std::vector<char>& bytes) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.exceptions(std::ios::badbit | std::ios::failbit);
    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}
#endif
template <typename Action>
void Reject(Action action) {
    bool failed = false;
    try {
        action();
    } catch (const std::exception&) {
        failed = true;
    }
    Check(failed);
}
}  // namespace
int main() {
    using namespace rhythm::security;
    try {
        TestDirectory directory;
        const auto path = directory.Path() / "host.identity";
        auto identity = HostIdentity::Create();
#ifdef _WIN32
        identity.SaveProtected(path);
        const auto original = Read(path);
        const auto pfx = detail::IdentityAccess::Pkcs12(identity);
        Check(std::search(original.begin(), original.end(), pfx.begin(), pfx.end(),
                          [](char left, std::uint8_t right) {
                              return static_cast<std::uint8_t>(left) == right;
                          }) == original.end());
        auto restored = HostIdentity::LoadProtected(path);
        Check(restored.Fingerprint() == identity.Fingerprint());
        Check(VerifyCertificate(restored.CertificateDer(), identity.Fingerprint()));
        restored.SaveProtected(path);
        Check(Read(path) != original);  // DPAPI uses randomized encryption.
        auto damaged = original;
        damaged.back() ^= 1;
        Write(path, damaged);
        Reject([&] { HostIdentity::LoadProtected(path); });
        Check(Read(path) == damaged);  // Failed load never replaces an existing identity.
        damaged = original;
        damaged[4] = 2;
        Write(path, damaged);
        Reject([&] { HostIdentity::LoadProtected(path); });
        Write(path, {'R', 'M'});
        Reject([&] { HostIdentity::LoadProtected(path); });
        Write(path, std::vector<char>(140000, 0));
        Reject([&] { HostIdentity::LoadProtected(path); });
        Write(path, original);
        {
            rhythm::storage::WriteGuard guard(directory.Path());
            Reject([&] { identity.SaveProtected(path); });
            Check(Read(path) == original);
        }
        auto replacement = HostIdentity::Create();
        replacement.SaveProtected(path);
        Check(HostIdentity::LoadProtected(path).Fingerprint() == replacement.Fingerprint());
        for (const auto& entry : std::filesystem::directory_iterator(directory.Path()))
            Check(entry.path().filename() == "host.identity" ||
                  entry.path().filename() == ".writer");
        std::cout << "vault contracts passed: current-user DPAPI, reload, tampering, bounds, "
                     "writer contention and atomic replacement\n";
#else
        Reject([&] { identity.SaveProtected(path); });
        Check(!std::filesystem::exists(path));
        std::cout << "host vault correctly unsupported on this participant platform\n";
#endif
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
