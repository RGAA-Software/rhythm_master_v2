#include "vault_file.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <stdexcept>

#include "rhythm/security/identity.h"
#include "rhythm/storage/atomic_file.h"

namespace rhythm::security::detail {
namespace {
constexpr std::array<std::uint8_t, 8> kHeader{'R', 'M', 'I', 'D', 1, 0, 0, 0};
class StagingDirectory final {
   public:
    explicit StagingDirectory(const std::filesystem::path& parent) {
        constexpr char kHex[] = "0123456789abcdef";
        std::string suffix;
        for (auto byte : GenerateToken()) {
            suffix += kHex[byte >> 4];
            suffix += kHex[byte & 15];
        }
        path_ = parent / (".identity-" + suffix);
        if (!std::filesystem::create_directory(path_))
            throw std::runtime_error("security.vault_staging");
    }
    ~StagingDirectory() {
        std::error_code ignored;
        // Remove only our known staging file and exclusive directory, never recurse.
        std::filesystem::remove(path_ / "identity", ignored);
        std::filesystem::remove(path_, ignored);
    }
    StagingDirectory(const StagingDirectory&) = delete;
    StagingDirectory& operator=(const StagingDirectory&) = delete;
    std::filesystem::path File() const { return path_ / "identity"; }

   private:
    std::filesystem::path path_{};
};
}  // namespace
void WriteVault(const std::filesystem::path& path, std::span<const std::uint8_t> secret) {
    auto bytes = ProtectSecret(secret);
    bytes.insert(bytes.begin(), kHeader.begin(), kHeader.end());
    const auto destination = std::filesystem::absolute(path);
    storage::WriteGuard guard(destination.parent_path());
    StagingDirectory staging(destination.parent_path());
    storage::WriteDurable(staging.File(),
                          {reinterpret_cast<const char*>(bytes.data()), bytes.size()});
    storage::Replace(staging.File(), destination);
}
SecretBytes ReadVault(const std::filesystem::path& path) {
    if (!std::filesystem::is_regular_file(std::filesystem::symlink_status(path)))
        throw std::runtime_error("security.vault_file");
    const auto size = std::filesystem::file_size(path);
    if (size <= kHeader.size() || size > 131072 + kHeader.size())
        throw std::runtime_error("security.vault_size");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    std::ifstream file(path, std::ios::binary);
    if (!file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size)) ||
        file.peek() != std::char_traits<char>::eof() ||
        !std::equal(kHeader.begin(), kHeader.end(), bytes.begin()))
        throw std::runtime_error("security.vault_format");
    return UnprotectSecret(std::span(bytes).subspan(kHeader.size()));
}
}  // namespace rhythm::security::detail
