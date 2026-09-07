#pragma once

#include "rhythm/export/settings.h"

namespace rhythm::exporting::detail {
// One freshly created private sibling of the destination on the same filesystem.
// This lease exclusively owns its contents, including partial media and music copies.
class Workspace final {
   public:
    explicit Workspace(const std::filesystem::path& destination);
    ~Workspace();
    Workspace(const Workspace&) = delete;
    Workspace& operator=(const Workspace&) = delete;
    const std::filesystem::path& Directory() const { return directory_; }
    void CopyMusic(const std::filesystem::path& source, std::stop_token stop) const;

   private:
    std::filesystem::path directory_{};
};
}  // namespace rhythm::exporting::detail
