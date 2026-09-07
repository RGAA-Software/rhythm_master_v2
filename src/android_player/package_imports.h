#pragma once

#include "rhythm/player/package_loader.h"

namespace rhythm::android_host {
// Owns only validated incoming-* files inside this app's private cache. One
// active import and one replaceable pending request; worker completion precedes
// file cleanup. It never removes the user's original document-provider file.
class PackageImports final {
   public:
    PackageImports(std::filesystem::path installed, std::filesystem::path cache);
    bool Request(std::filesystem::path path);
    std::optional<player::PackageLoadResult> Take();
    bool Busy() const { return loader_.Busy() || pending_.has_value(); }

   private:
    struct FileLease {
        explicit FileLease(std::filesystem::path path) : path_(std::move(path)) {}
        ~FileLease();
        FileLease(FileLease&& other) noexcept;
        FileLease(const FileLease&) = delete;
        FileLease& operator=(const FileLease&) = delete;
        std::filesystem::path path_{};
    };
    bool StartPending();
    std::filesystem::path installed_{};
    std::filesystem::path cache_{};
    std::optional<FileLease> active_{};
    std::optional<FileLease> pending_{};
    // Destroyed first: stops/joins worker before either file lease is released.
    player::PackageLoader loader_{};
};
}  // namespace rhythm::android_host
