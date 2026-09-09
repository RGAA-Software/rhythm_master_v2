#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "rhythm/shader_artifact/artifact.h"

namespace rhythm::image_shader {
using Target = shader_artifact::Target;
struct Program {
    std::string expression_{};
    std::string compiler_sha256_{};
    std::array<std::vector<std::uint8_t>, 2> artifacts_{};
};
inline constexpr std::size_t kMaximumArtifactBytes = shader_artifact::kMaximumArtifactBytes;
// Profile-bound artifacts, not arbitrary native programs. Validation covers
// container structure and fixed bindings; bytecode execution still belongs to
// the selected graphics driver. Only the authoring compiler produces artifacts.
void ValidateArtifact(std::span<const std::uint8_t> bytes, Target target);
void Validate(const Program& program);
std::vector<std::uint8_t> Encode(const Program& program);
Program Decode(std::span<const std::uint8_t> bytes);
}  // namespace rhythm::image_shader
