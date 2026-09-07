#pragma once

#include <filesystem>
#include <span>
#include <string_view>

#include "rhythm/assets/types.h"
#include "rhythm/graph/compiler.h"
#include "rhythm/media/soundtrack.h"

namespace rhythm::project {
inline constexpr std::size_t kMaximumProgramBytes = 8 * 1024 * 1024;
inline constexpr std::size_t kMaximumPackageBytes = 16 * 1024 * 1024;
inline constexpr std::size_t kMaximumPackageAssets = 64;
inline constexpr std::size_t kMaximumPackageAssetBytes = 8 * 1024 * 1024;
enum class PackageProfile {
    kTextureSignalV1,
    kTextureSignalAssetsV1,
    kTextureSignalV2,
    kMusicPerformanceV1
};
struct PackagedAsset {
    assets::AssetRecord record_{};
    std::string bytes_{};
    bool operator==(const PackagedAsset&) const = default;
};
struct RuntimePackage {
    graph::ExecutionPlan program_{};
    std::string title_{};
    std::vector<PackagedAsset> assets_{};
    PackageProfile profile_ = PackageProfile::kTextureSignalV2;
    std::optional<media::Soundtrack> soundtrack_{};
};
// Player-facing serialization has no editor, UI, Protobuf or native API types.
std::string EncodeProgram(const graph::ExecutionPlan& plan);
graph::ExecutionPlan DecodeProgram(std::string_view bytes, std::uint32_t required_abi = 0);
std::string EncodePackage(const graph::Document& document, std::string_view title,
                          std::span<const PackagedAsset> assets = {},
                          const std::optional<media::Soundtrack>& soundtrack = {});
RuntimePackage DecodePackage(std::string_view bytes);
RuntimePackage LoadPackage(const std::filesystem::path& path);
// Untrusted incoming bytes are validated before replacing installed content.
void InstallPackage(const std::filesystem::path& path, std::string_view bytes,
                    bool fail_before_commit = false);
// Atomic replacement; a failed publish leaves the previous file intact.
void PublishPackage(const std::filesystem::path& path, const graph::Document& document,
                    std::string_view title, bool fail_before_commit = false);
}  // namespace rhythm::project
