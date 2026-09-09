#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "rhythm/shader_expression/source.h"

namespace rhythm::image_shader {
// The image-expression profile is a single RGBA expression. Actual GLSL typing
// belongs to the existing shader compiler; this layer bounds syntax/resources.
using Diagnostic = shader_expression::Diagnostic;
inline constexpr std::size_t kMaximumSourceBytes = shader_expression::kMaximumSourceBytes;
std::optional<Diagnostic> ValidateExpression(std::string_view expression);
std::string FragmentSource(std::string_view expression);
}  // namespace rhythm::image_shader
