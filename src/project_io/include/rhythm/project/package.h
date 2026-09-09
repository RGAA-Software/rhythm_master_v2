#pragma once

#include <filesystem>
#include <span>
#include <stop_token>
#include <string_view>

#include "rhythm/assets/types.h"
#include "rhythm/graph/compiler.h"
#include "rhythm/media/soundtrack.h"
#include "rhythm/storage/file_bytes.h"

namespace rhythm::project {
inline constexpr std::size_t kMaximumProgramBytes = 8 * 1024 * 1024;
inline constexpr std::size_t kMaximumPackageBytes = 48 * 1024 * 1024;
inline constexpr std::size_t kMaximumPackageAssets = 64;
// Includes complete redistributable CJK fonts; shared by all ordinary assets.
inline constexpr std::size_t kMaximumPackageAssetBytes = 32 * 1024 * 1024;
inline constexpr std::uint64_t kMaximumMusicAssetBytes = 256 * 1024 * 1024;
inline constexpr std::uint64_t kMaximumFilePackageBytes = 304 * 1024 * 1024;
enum class PackageProfile {
    kTextureSignalV1,
    kTextureSignalAssetsV1,
    kTextureSignalV2,
    kMusicPerformanceV1,
    kMusicPerformanceV2,
    kMusicArrangementV1
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
    struct StreamedAudio {
        assets::AssetRecord record_{};
        storage::FileBytes bytes_{};
    };
    std::optional<StreamedAudio> streamed_audio_{};
};
// Player-facing serialization has no editor, UI, Protobuf or native API types.
std::string EncodeProgram(const graph::ExecutionPlan& plan);
graph::ExecutionPlan DecodeProgram(std::string_view bytes, std::uint32_t required_abi = 0);
std::string EncodePackage(const graph::Document& document, std::string_view title,
                          std::span<const PackagedAsset> assets = {},
                          const std::optional<media::Soundtrack>& soundtrack = {});
RuntimePackage DecodePackage(std::string_view bytes);
RuntimePackage ReadPackage(storage::FileBytes source, std::stop_token stop = {});
RuntimePackage LoadPackage(const std::filesystem::path& path, std::stop_token stop = {});
// Validates the shared publication/template/import asset budgets. Only the bound
// soundtrack may use the separate music allowance; ordinary assets share 32 MiB.
bool RequiresStreamedAudio(std::span<const assets::AssetRecord> records,
                           const std::optional<media::Soundtrack>& soundtrack);
// Worker-only large-song publication. The ordinary asset/program budgets remain
// unchanged; one validated stored music range has its own explicit profile.
void PublishMusicPackage(const std::filesystem::path& path, const graph::Document& document,
                         std::string_view title, std::span<const PackagedAsset> assets,
                         const RuntimePackage::StreamedAudio& audio,
                         const media::Soundtrack& soundtrack, bool fail_before_commit = false,
                         std::stop_token stop = {});
// Untrusted incoming bytes are validated before replacing installed content.
void InstallPackage(const std::filesystem::path& path, std::string_view bytes,
                    bool fail_before_commit = false);
void InstallPackageFile(const std::filesystem::path& path, storage::FileBytes source,
                        bool fail_before_commit = false, std::stop_token stop = {});
// Atomic replacement; a failed publish leaves the previous file intact.
void PublishPackage(const std::filesystem::path& path, const graph::Document& document,
                    std::string_view title, bool fail_before_commit = false);
}  // namespace rhythm::project
