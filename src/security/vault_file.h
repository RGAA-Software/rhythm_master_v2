#pragma once

#include <filesystem>

#include "secret_bytes.h"

namespace rhythm::security::detail {
SecretBytes ReadVault(const std::filesystem::path& path);
void WriteVault(const std::filesystem::path& path, std::span<const std::uint8_t> secret);
}  // namespace rhythm::security::detail
