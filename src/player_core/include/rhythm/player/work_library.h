#pragma once

#include <filesystem>
#include <stop_token>

#include "rhythm/performance/list.h"
#include "rhythm/storage/file_bytes.h"

namespace rhythm::player {
struct StoredWork {
    performance::WorkReference reference_{};
    std::string title_{};
};
// Blocking worker-side, application-owned immutable package storage. Reuses the
// asset store's bounded hash/copy/repair transactions; no playback or GPU state.
// A source may be removed after Import returns. Reopen validates the stored hash.
class WorkLibrary final {
   public:
    explicit WorkLibrary(std::filesystem::path directory);
    // Without an expected reference, creates a managed identity. An expected
    // reference imports/restores only those exact bytes (including built-ins).
    // A rejected source never returns a publishable work reference.
    StoredWork Import(const std::filesystem::path& source,
                      const std::optional<performance::WorkReference>& expected = {},
                      std::stop_token stop = {});
    storage::FileBytes Open(const performance::WorkReference& work,
                            std::stop_token stop = {}) const;

   private:
    std::filesystem::path directory_{};
};
}  // namespace rhythm::player
