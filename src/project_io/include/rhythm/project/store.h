#pragma once

#include <filesystem>
#include <span>

#include "rhythm/editor/history.h"

namespace rhythm::project {
// Limits apply before parsing; schema recursion is bounded by the codec.
inline constexpr std::size_t kMaximumGraphBytes = 8 * 1024 * 1024;
enum class CommitStep {
    kNone,
    kGraphWritten,
    kEditorWritten,
    kManifestWritten,
    kValidated,
    kBeforeCommit,
    kCommitted
};
std::string EncodeGraph(const graph::Document& document);
graph::Document DecodeGraph(std::string_view bytes);
std::string Digest(std::string_view bytes);
enum class AssetValidation { kStrict, kAllowRepair };
struct LoadResult {
    editor::Snapshot snapshot_{};
    std::vector<graph::Diagnostic> warnings_{};
    std::vector<assets::AssetId> unavailable_assets_{};
};
struct ContentEntry {
    std::string id_{};
    std::string version_{};
    std::filesystem::path directory_{};
    std::map<std::string, std::string> titles_{};
    bool default_ = false;
    std::string tier_ = "example";
    std::string category_ = "general";
    std::map<std::string, std::string> descriptions_{};
};
std::vector<ContentEntry> ScanTemplates(const std::filesystem::path& root);
LoadResult LoadRevision(const std::filesystem::path& directory);
// Repair mode preserves authored records and reports unavailable content.
// Structural/path validation remains strict; save/publish never use this mode.
LoadResult Load(const std::filesystem::path& project,
                AssetValidation validation = AssetValidation::kStrict);
LoadResult PrepareTemplate(const std::filesystem::path& directory,
                           const std::filesystem::path& asset_directory);
// Resolves immutable blobs on the calling worker; no source paths enter the package.
void PublishSnapshot(const std::filesystem::path& path, const editor::Snapshot& snapshot,
                     const std::filesystem::path& asset_directory = {});
// Single-writer transaction. Fault injection is explicit and defaults to disabled.
void Save(const std::filesystem::path& project, const editor::Snapshot& snapshot,
          CommitStep fail_after = CommitStep::kNone);
}  // namespace rhythm::project
