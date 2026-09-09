#pragma once

#include <filesystem>
#include <string>
#include <string_view>

#include "rhythm/performance/list.h"

namespace rhythm::project {
std::string EncodePerformanceList(const performance::List& list);
performance::List DecodePerformanceList(std::string_view bytes);
enum class PerformanceCommitStep { kNone, kWritten, kValidated };
// Blocking worker-side I/O. A private application directory contains list.json;
// saving holds its existing atomic_storage writer lease and atomically replaces
// the document only after reopening the staged bytes. Failure keeps the old list.
// Missing/corrupt/future documents throw; hosts must not overwrite them with defaults.
performance::List LoadPerformanceList(const std::filesystem::path& directory);
void SavePerformanceList(const std::filesystem::path& directory, const performance::List& list,
                         PerformanceCommitStep fail_after = PerformanceCommitStep::kNone);
}  // namespace rhythm::project
