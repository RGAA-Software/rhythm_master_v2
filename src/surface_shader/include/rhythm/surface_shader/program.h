#pragma once

#include <array>
#include <map>
#include <span>
#include <string>
#include <vector>

#include "rhythm/shader_artifact/artifact.h"
#include "rhythm/shader_expression/source.h"

namespace rhythm::surface_shader {
struct Program {
    std::string expression_{};
    std::string compiler_sha256_{};
    std::array<std::vector<std::uint8_t>, 2> artifacts_{};
};
// Immutable worker result; render-thread program ownership is separate.
struct Resources {
    std::map<std::string, Program> programs_{};
};
inline constexpr std::string_view kMediaType = "application/x-rhythm-surface-shader";
inline constexpr std::size_t kMaximumPrograms = 32;
inline constexpr std::size_t kMaximumProgramBytes = 16 * 1024 * 1024;
inline constexpr std::size_t kMaximumSourceBytes = shader_expression::kMaximumSourceBytes;
inline constexpr std::size_t kMaximumArtifactBytes = shader_artifact::kMaximumArtifactBytes;
// RGB multiplier after base-texture sampling and before lighting. No alpha,
// geometry, sampler additions or user-controlled light/shadow bindings.
std::string FragmentSource(std::string_view expression);
void Validate(const Program& program);
std::vector<std::uint8_t> Encode(const Program& program);
Program Decode(std::span<const std::uint8_t> bytes);
}  // namespace rhythm::surface_shader
