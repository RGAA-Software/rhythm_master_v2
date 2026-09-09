#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace rhythm::shader_artifact {
enum class Target : std::uint8_t { kWindowsSm5, kGles300 };
enum class Profile : std::uint8_t { kImageRgba, kSurfaceRgb };
inline constexpr std::size_t kMaximumArtifactBytes = 512 * 1024;
// Validates the fixed shader container and binding contract, not arbitrary
// native-code safety or equivalence of a binary to author-declared source.
void Validate(std::span<const std::uint8_t> bytes, Target target, Profile profile);
}  // namespace rhythm::shader_artifact
