#pragma once

#include <array>
#include <map>

#include "rhythm/export/jobs.h"

namespace rhythm::studio {
struct ExportRequest {
    exporting::ExportSettings settings_{};
    std::filesystem::path destination_{};
};
class ExportPanel final {
   public:
    void Open(const std::filesystem::path& destination,
              const std::optional<std::filesystem::path>& music, double duration, float gain,
              graph::Canvas canvas);
    std::optional<ExportRequest> Draw(const std::map<std::string, std::string>& text);
    void Start(const std::filesystem::path& executable, editor::Snapshot snapshot,
               const std::filesystem::path& assets, ExportRequest request);
    exporting::JobSnapshot Snapshot() const;

   private:
    void SetPath(std::array<char, 4096>& buffer, const std::filesystem::path& path);
    std::optional<exporting::ExportJobs> jobs_{};
    std::array<char, 4096> music_{};
    std::array<char, 4096> destination_{};
    graph::Canvas canvas_{};
    std::string error_{};
    float duration_ = 10;
    float gain_ = 1;
    int fps_ = 0;
    int scale_ = 0;
    int codec_ = 0;
    int quality_ = 1;
    bool open_ = false;
};
}  // namespace rhythm::studio
