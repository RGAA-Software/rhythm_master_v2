#include <stdexcept>

#include "rhythm/assets/store.h"
#include "rhythm/project/package.h"
#include "rhythm/project/store.h"

namespace rhythm::project {
LoadResult PrepareTemplate(const std::filesystem::path& directory,
                           const std::filesystem::path& asset_directory) {
    auto result = LoadRevision(directory);
    if (!std::holds_alternative<graph::ExecutionPlan>(
                graph::Compile(result.snapshot_.document_, graph::Registry{})))
        throw std::invalid_argument("graph.template_invalid");
    if (result.snapshot_.assets_.size() > kMaximumPackageAssets)
        throw std::length_error("package.asset_count");
    if (!result.snapshot_.assets_.empty()) {
        if (!std::filesystem::is_directory(directory / "assets") ||
            std::filesystem::is_symlink(directory / "assets"))
            throw std::invalid_argument("project.asset_directory");
        const assets::Store source(directory / "assets");
        assets::Store destination(asset_directory);
        std::uint64_t remaining = kMaximumPackageAssetBytes;
        for (const auto& asset : result.snapshot_.assets_) {
            destination.CopyFrom(source, asset, remaining);
            remaining -= asset.bytes_;
        }
    }
    return result;
}
}  // namespace rhythm::project
