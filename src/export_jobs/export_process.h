#pragma once

#include <filesystem>
#include <memory>

namespace rhythm::exporting::detail {
// SDL's private process adapter. The worker owns the child; destruction kills
// and reaps it before any staging files can be removed. No shell interpolation.
class ExportProcess final {
   public:
    ExportProcess(const std::filesystem::path& executable, const std::filesystem::path& directory);
    ~ExportProcess();
    ExportProcess(const ExportProcess&) = delete;
    ExportProcess& operator=(const ExportProcess&) = delete;
    bool Done();

   private:
    class Impl;
    std::unique_ptr<Impl> impl_{};
};
}  // namespace rhythm::exporting::detail
