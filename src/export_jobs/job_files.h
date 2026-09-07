#pragma once

#include "rhythm/export/settings.h"

namespace rhythm::exporting::detail {
struct JobResult {
    bool success_ = false;
    std::string error_{};
};
void WriteRequest(const std::filesystem::path& directory, const ExportSettings& settings);
ExportSettings ReadRequest(const std::filesystem::path& directory);
void WriteProgress(const std::filesystem::path& directory, ExportProgress progress);
std::optional<ExportProgress> ReadProgress(const std::filesystem::path& directory);
void WriteResult(const std::filesystem::path& directory, const JobResult& result);
JobResult ReadResult(const std::filesystem::path& directory);
}  // namespace rhythm::exporting::detail
