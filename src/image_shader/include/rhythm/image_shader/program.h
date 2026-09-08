#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace rhythm::image_shader {
enum class Target : std::uint8_t { kWindowsSm5, kGles300 };
struct Program {
    std::string expression_{};
    std::string compiler_sha256_{};
    std::array<std::vector<std::uint8_t>, 2> artifacts_{};
};
inline constexpr std::size_t kMaximumArtifactBytes = 512 * 1024;
// Profile-bound artifacts, not arbitrary native programs. Validation covers
// container structure and fixed bindings; bytecode execution still belongs to
// the selected graphics driver. Only the authoring compiler produces artifacts.
void ValidateArtifact(std::span<const std::uint8_t> bytes, Target target);
void Validate(const Program& program);
std::vector<std::uint8_t> Encode(const Program& program);
Program Decode(std::span<const std::uint8_t> bytes);
}  // namespace rhythm::image_shader
