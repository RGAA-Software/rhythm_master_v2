#include "secret_bytes.h"

#include <openssl/crypto.h>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace rhythm::security::detail {
SecretBytes::SecretBytes(std::size_t size) {
    if (size > 65536) throw std::length_error("security.secret_size");
    bytes_.resize(size);
}
SecretBytes::SecretBytes(std::span<const std::uint8_t> bytes) : SecretBytes(bytes.size()) {
    std::copy(bytes.begin(), bytes.end(), bytes_.begin());
}
SecretBytes::~SecretBytes() { Clear(); }
SecretBytes::SecretBytes(SecretBytes&& other) noexcept : bytes_(std::exchange(other.bytes_, {})) {}
SecretBytes& SecretBytes::operator=(SecretBytes&& other) noexcept {
    if (this != &other) {
        Clear();
        bytes_ = std::exchange(other.bytes_, {});
    }
    return *this;
}
void SecretBytes::Clear() {
    if (!bytes_.empty()) OPENSSL_cleanse(bytes_.data(), bytes_.size());
    bytes_.clear();
}
}  // namespace rhythm::security::detail
