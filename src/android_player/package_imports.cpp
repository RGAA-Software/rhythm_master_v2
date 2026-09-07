#include "package_imports.h"

#include <utility>

namespace rhythm::android_host {
PackageImports::FileLease::~FileLease() {
    if (!path_.empty()) {
        std::error_code error;
        std::filesystem::remove(path_, error);
    }
}
PackageImports::FileLease::FileLease(FileLease&& other) noexcept
    : path_(std::exchange(other.path_, {})) {}
PackageImports::PackageImports(std::filesystem::path installed, std::filesystem::path cache)
    : installed_(std::move(installed)), cache_(std::filesystem::canonical(cache)) {}
bool PackageImports::Request(std::filesystem::path path) {
    try {
        if (std::filesystem::symlink_status(path).type() != std::filesystem::file_type::regular ||
            std::filesystem::canonical(path.parent_path()) != cache_ ||
            !path.filename().string().starts_with("incoming-") || path.extension() != ".rhythmpack")
            return false;
        const auto owned = cache_ / path.filename();
        if ((active_ && active_->path_ == owned) || (pending_ && pending_->path_ == owned))
            return false;
        pending_.emplace(owned);
        return loader_.Busy() || StartPending();
    } catch (const std::exception&) {
        return false;
    }
}
bool PackageImports::StartPending() {
    if (!pending_ || loader_.Busy()) return false;
    active_.emplace(std::move(*pending_));
    pending_.reset();
    if (loader_.StartFile(active_->path_, installed_)) return true;
    active_.reset();
    return false;
}
std::optional<player::PackageLoadResult> PackageImports::Take() {
    auto result = loader_.Take();
    if (!result) return std::nullopt;
    active_.reset();
    if (pending_) StartPending();
    return result;
}
}  // namespace rhythm::android_host
