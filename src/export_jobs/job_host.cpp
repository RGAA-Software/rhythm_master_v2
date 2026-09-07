#include <chrono>

#include "job_files.h"
#include "rhythm/export/jobs.h"
#include "rhythm/export/render_export.h"
#include "rhythm/platform/host.h"

namespace rhythm::exporting {
int RunExportJob(const std::filesystem::path& directory) {
    if (std::filesystem::exists(directory / "result.json") ||
        std::filesystem::exists(directory / "output.mp4"))
        return 1;
    try {
        const auto settings = detail::ReadRequest(directory);
        const auto package = project::LoadPackage(directory / "source.rhythmpack");
        {
            platform::Host host(true);
            auto renderer = host.CreateRenderer();
            auto updated = std::chrono::steady_clock::time_point{};
            RenderExport(package, settings, directory / "output.mp4", renderer,
                         [&](ExportProgress progress) {
                             const auto now = std::chrono::steady_clock::now();
                             if (progress.completed_frames_ == progress.total_frames_ ||
                                 now - updated >= std::chrono::milliseconds(250)) {
                                 detail::WriteProgress(directory, progress);
                                 updated = now;
                             }
                         });
        }
        detail::WriteResult(directory, {true, {}});
        return 0;
    } catch (const std::exception& error) {
        detail::WriteResult(directory, {false, error.what()});
        return 1;
    }
}
}  // namespace rhythm::exporting
