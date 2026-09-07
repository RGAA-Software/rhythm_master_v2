#pragma once

#include <filesystem>

namespace rhythm::media::test {
// Test-only synthetic content generated through the installed media backend.
void WriteFlac(const std::filesystem::path& path);
}  // namespace rhythm::media::test
