#pragma once

#include <map>

#include "rhythm/image_shader/program.h"

namespace rhythm::image_shader {
// Immutable worker result, indexed by content SHA-256. GPU ownership stays in Runtime.
struct Resources {
    std::map<std::string, Program> programs_{};
};
inline constexpr std::string_view kMediaType = "application/x-rhythm-image-shader";
inline constexpr std::size_t kMaximumPrograms = 64;
inline constexpr std::size_t kMaximumProgramBytes = 16 * 1024 * 1024;
}  // namespace rhythm::image_shader
