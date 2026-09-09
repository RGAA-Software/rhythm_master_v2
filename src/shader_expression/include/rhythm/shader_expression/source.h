#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace rhythm::shader_expression {
enum class Profile : std::uint8_t { kImageRgba, kSurfaceRgb };
struct Diagnostic {
    std::string code_{};
    std::uint32_t line_ = 1;
    std::uint32_t column_ = 1;
};
inline constexpr std::size_t kMaximumSourceBytes = 8192;
// Syntax/resource admission only. The existing shader compiler checks actual
// expression types against the selected wrapper's return/input types.
std::optional<Diagnostic> Validate(std::string_view expression, Profile profile);
}  // namespace rhythm::shader_expression
