#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace rhythm::security::detail {
// Private adapter memory, never exposed to graph/UI/persistence public APIs.
class SecretBytes final {
   public:
    SecretBytes() = default;
    explicit SecretBytes(std::size_t size);
    explicit SecretBytes(std::span<const std::uint8_t> bytes);
    ~SecretBytes();
    SecretBytes(SecretBytes&& other) noexcept;
    SecretBytes& operator=(SecretBytes&& other) noexcept;
    SecretBytes(const SecretBytes&) = delete;
    SecretBytes& operator=(const SecretBytes&) = delete;
    std::span<const std::uint8_t> Bytes() const { return bytes_; }
    std::span<std::uint8_t> MutableBytes() { return bytes_; }

   private:
    void Clear();
    std::vector<std::uint8_t> bytes_{};
};
std::vector<std::uint8_t> ProtectSecret(std::span<const std::uint8_t> bytes);
SecretBytes UnprotectSecret(std::span<const std::uint8_t> bytes);
}  // namespace rhythm::security::detail
