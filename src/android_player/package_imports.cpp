#include "package_imports.h"

#include <utility>

namespace rhythm::android_host {
PackageImports::PackageImports(std::filesystem::path installed, std::filesystem::path cache)
    : installed_(std::move(installed)), cache_(std::filesystem::canonical(cache)) {}
bool PackageImports::Request(std::filesystem::path path) {
    try {
        const auto owned = std::filesystem::weakly_canonical(path);
        if ((active_ && active_->Path() == owned) || (pending_ && pending_->Path() == owned))
            return false;
        auto file = ImportFile::Claim(path, cache_);
        if (!file) return false;
        pending_.emplace(std::move(*file));
        return loader_.Busy() || StartPending();
    } catch (const std::exception&) {
        return false;
    }
}
bool PackageImports::StartPending() {
    if (!pending_ || loader_.Busy()) return false;
    active_.emplace(std::move(*pending_));
    pending_.reset();
    if (loader_.StartFile(active_->Path(), installed_)) return true;
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
