#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace rhythm::image_shader {
// The image-expression profile is a single RGBA expression. Actual GLSL typing
// belongs to the existing shader compiler; this layer bounds syntax/resources.
struct Diagnostic {
    std::string code_{};
    std::uint32_t line_ = 1;
    std::uint32_t column_ = 1;
};
inline constexpr std::size_t kMaximumSourceBytes = 8192;
std::optional<Diagnostic> ValidateExpression(std::string_view expression);
std::string FragmentSource(std::string_view expression);
}  // namespace rhythm::image_shader
