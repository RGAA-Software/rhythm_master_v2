#pragma once

#include <filesystem>
#include <stop_token>
#include <string>
#include <vector>

namespace rhythm::shader_authoring::detail {
// Blocking worker-only native process boundary, bounded by time/output/cancellation.
void RunCompiler(const std::vector<std::string>& arguments, const std::filesystem::path& log,
                 std::stop_token stop);
}  // namespace rhythm::shader_authoring::detail
