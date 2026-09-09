#include <picosha2.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <nlohmann/json.hpp>
#include <set>
#include <stdexcept>

#include "layout_codec.h"
#include "rhythm/assets/store.h"
#include "rhythm/project/store.h"
#include "rhythm/storage/atomic_file.h"
#include "rhythm/storage/file_bytes.h"
#include "soundtrack_codec.h"

namespace rhythm::project {
namespace {
using Json = nlohmann::json;
std::atomic<std::uint64_t> revision_counter{0};
Json ParseMetadata(std::string_view bytes) {
    std::vector<std::set<std::string>> object_keys;
    return Json::parse(bytes, [&](int depth, Json::parse_event_t event, Json& value) {
        if (depth > 32) throw std::length_error("project.metadata_depth");
        if (event == Json::parse_event_t::object_start) object_keys.emplace_back();
        if (event == Json::parse_event_t::key &&
            !object_keys.back().insert(value.get<std::string>()).second)
            throw std::invalid_argument("project.metadata_duplicate_key");
        if (event == Json::parse_event_t::object_end) object_keys.pop_back();
        return true;
    });
}
std::string Read(const std::filesystem::path& path, std::size_t maximum) {
    if (std::filesystem::is_symlink(path)) throw std::invalid_argument("project.symlink");
    // Size and bytes belong to one open-file lease. CURRENT may be atomically
    // replaced between path operations; a path-sized ifstream is not a snapshot.
    const auto file = storage::FileBytes::Open(path, maximum);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(file.Size()));
    file.Read(0, bytes);
    return {bytes.begin(), bytes.end()};
}
bool SafeRevision(std::string_view revision) {
    return !revision.empty() && revision.size() <= 96 &&
           std::all_of(revision.begin(), revision.end(), [](char character) {
               return (character >= '0' && character <= '9') ||
                      (character >= 'a' && character <= 'f') || character == '-';
           });
}
void Fault(CommitStep current, CommitStep requested) {
    if (current == requested) throw std::runtime_error("project.injected_failure");
}
Json EncodeAssets(const std::vector<assets::AssetRecord>& assets) {
    if (assets.size() > 4096) throw std::length_error("project.asset_count");
    Json records = Json::array();
    std::set<std::string> ids;
    for (const auto& asset : assets) {
        if (!assets::ValidId(asset.id_) || !assets::ValidMediaType(asset.media_type_) ||
            asset.bytes_ > 1024ull * 1024 * 1024 || !ids.insert(asset.id_.sha256_).second)
            throw std::invalid_argument("project.asset_record");
        records.push_back({{"sha256", asset.id_.sha256_},
                           {"bytes", asset.bytes_},
                           {"media_type", asset.media_type_}});
    }
    return records;
}
void VerifyAssets(const std::filesystem::path& project,
                  const std::vector<assets::AssetRecord>& assets) {
    if (assets.empty()) return;
    const auto directory = project / "assets";
    if (!std::filesystem::is_directory(directory) || std::filesystem::is_symlink(directory))
        throw std::invalid_argument("project.asset_directory");
    const assets::Store store(directory);
    for (const auto& asset : assets)
        if (!store.Verify(asset)) throw std::invalid_argument("project.asset_integrity");
}
}  // namespace
std::string Digest(std::string_view bytes) {
    return picosha2::hash256_hex_string(bytes.begin(), bytes.end());
}

std::vector<ContentEntry> ScanTemplates(const std::filesystem::path& root) {
    std::vector<ContentEntry> result;
    std::set<std::string> ids;
    for (const auto& entry : std::filesystem::directory_iterator(root)) {
        if (!entry.is_directory() || entry.is_symlink()) continue;
        const auto manifest = ParseMetadata(Read(entry.path() / "manifest.json", 1024 * 1024));
        const auto version = manifest.at("manifest_version");
        if (manifest.at("kind") != "template" || (version != 1 && version != 2 && version != 3))
            throw std::invalid_argument("content.version");
        ContentEntry content{manifest.at("content_id").get<std::string>(),
                             manifest.at("content_version").get<std::string>(), entry.path()};
        if (content.id_.empty() || content.version_.empty() || !ids.insert(content.id_).second)
            throw std::invalid_argument("content.identity");
        content.titles_ = manifest.at("titles").get<std::map<std::string, std::string>>();
        content.default_ = manifest.value("default", false);
        content.tier_ = manifest.value("tier", "example");
        content.category_ = manifest.value("category", "general");
        if (content.tier_ != "basic" && content.tier_ != "advanced" && content.tier_ != "example")
            throw std::invalid_argument("content.tier");
        if (content.category_.empty() || content.category_.size() > 64)
            throw std::invalid_argument("content.category");
        if (manifest.contains("descriptions"))
            content.descriptions_ =
                    manifest.at("descriptions").get<std::map<std::string, std::string>>();
        for (const auto& locale : {"zh-CN", "en-US"})
            if (!content.titles_.contains(locale) || content.titles_.at(locale).empty() ||
                content.titles_.at(locale).size() > 512)
                throw std::invalid_argument("content.locale");
        for (const auto& [locale, description] : content.descriptions_)
            if (description.size() > 2048) throw std::invalid_argument("content.description");
        result.push_back(std::move(content));
    }
    std::sort(result.begin(), result.end(),
              [](const auto& left, const auto& right) { return left.id_ < right.id_; });
    return result;
}
LoadResult LoadRevision(const std::filesystem::path& directory) {
    if (std::filesystem::is_symlink(directory)) throw std::invalid_argument("project.symlink");
    const auto manifest = ParseMetadata(Read(directory / "manifest.json", 1024 * 1024));
    const auto version = manifest.at("manifest_version");
    if (manifest.at("format") != "rhythm.project" || (version != 1 && version != 2 && version != 3))
        throw std::invalid_argument("project.manifest_version");
    const auto bytes = Read(directory / "graph.pb", kMaximumGraphBytes);
    if (manifest.at("graph_sha256") != Digest(bytes))
        throw std::invalid_argument("project.graph_hash");
    LoadResult result;
    result.snapshot_.document_ = DecodeGraph(bytes);
    if (result.snapshot_.document_.id_ != manifest.at("project_id").get<std::string>() ||
        result.snapshot_.document_.revision_ != manifest.at("graph_revision").get<std::uint64_t>())
        throw std::invalid_argument("project.identity");
    result.snapshot_.title_ = manifest.at("title").get<std::string>();
    if (result.snapshot_.title_.size() > 4096) throw std::length_error("project.metadata_limit");
    if (manifest.contains("assets")) {
        const auto& records = manifest.at("assets");
        if (!records.is_array() || records.size() > 4096)
            throw std::invalid_argument("project.asset_count");
        for (const auto& record : records) {
            if (!record.at("bytes").is_number_unsigned())
                throw std::invalid_argument("project.asset_record");
            result.snapshot_.assets_.push_back({{record.at("sha256").get<std::string>()},
                                                record.at("bytes").get<std::uint64_t>(),
                                                record.at("media_type").get<std::string>()});
        }
        EncodeAssets(result.snapshot_.assets_);
    }
    if (version == 2 || version == 3) {
        result.snapshot_.soundtrack_ =
                detail::DecodeSoundtrack(manifest.at("soundtrack"), result.snapshot_.assets_);
        if ((version == 3) != !result.snapshot_.soundtrack_->clips_.empty())
            throw std::invalid_argument("project.soundtrack_version");
    } else if (manifest.contains("soundtrack"))
        throw std::invalid_argument("project.soundtrack_version");
    try {
        const auto layout_bytes = Read(directory / "editor.json", 1024 * 1024);
        if (manifest.at("editor_sha256") != Digest(layout_bytes))
            throw std::invalid_argument("project.editor_hash");
        const auto layout = ParseMetadata(layout_bytes);
        DecodeLayout(layout, result.snapshot_);
    } catch (const std::exception&) {
        result.snapshot_.positions_.clear();
        result.snapshot_.component_positions_.clear();
        result.warnings_.push_back({"project.layout_reset"});
    }
    return result;
}
LoadResult Load(const std::filesystem::path& project, AssetValidation validation) {
    const auto revision = Read(project / "CURRENT", 96);
    if (!SafeRevision(revision) || std::filesystem::is_symlink(project / "revisions"))
        throw std::invalid_argument("project.current");
    const auto directory = project / "revisions" / revision;
    const auto manifest = ParseMetadata(Read(directory / "manifest.json", 1024 * 1024));
    if (manifest.at("revision_id") != revision)
        throw std::invalid_argument("project.revision_mismatch");
    auto result = LoadRevision(directory);
    if (validation == AssetValidation::kStrict) {
        VerifyAssets(project, result.snapshot_.assets_);
    } else if (!result.snapshot_.assets_.empty()) {
        const auto assets = project / "assets";
        if (std::filesystem::is_symlink(assets) ||
            (std::filesystem::exists(assets) && !std::filesystem::is_directory(assets)))
            throw std::invalid_argument("project.asset_directory");
        if (!std::filesystem::exists(assets)) {
            for (const auto& record : result.snapshot_.assets_)
                result.unavailable_assets_.push_back(record.id_);
        } else {
            const rhythm::assets::Store store(assets);
            for (const auto& record : result.snapshot_.assets_)
                if (!store.Verify(record)) result.unavailable_assets_.push_back(record.id_);
        }
        if (!result.unavailable_assets_.empty())
            result.warnings_.push_back({"project.assets_need_repair"});
    }
    return result;
}
void Save(const std::filesystem::path& project, const editor::Snapshot& snapshot,
          CommitStep fail_after) {
    const auto graph_bytes = EncodeGraph(snapshot.document_);
    const auto editor_bytes = EncodeLayout(snapshot).dump(2);
    const auto assets = EncodeAssets(snapshot.assets_);
    const auto soundtrack =
            snapshot.soundtrack_ ? detail::EncodeSoundtrack(*snapshot.soundtrack_, snapshot.assets_)
                                 : Json{};
    VerifyAssets(project, snapshot.assets_);
    if (editor_bytes.size() > 1024 * 1024 || snapshot.title_.size() > 4096)
        throw std::length_error("project.metadata_limit");
    std::filesystem::create_directories(project);
    if (std::filesystem::is_symlink(project) ||
        std::filesystem::is_symlink(project / "revisions") ||
        std::filesystem::is_symlink(project / ".writer"))
        throw std::invalid_argument("project.symlink");
    const storage::WriteGuard lock(project);
    const auto revision =
            Digest(graph_bytes).substr(0, 16) + "-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" +
            std::to_string(revision_counter.fetch_add(1));
    std::filesystem::create_directories(project / "revisions");
    const auto directory = project / "revisions" / revision;
    if (!std::filesystem::create_directory(directory))
        throw std::runtime_error("project.revision_collision");
    storage::WriteDurable(directory / "graph.pb", graph_bytes);
    Fault(CommitStep::kGraphWritten, fail_after);
    storage::WriteDurable(directory / "editor.json", editor_bytes);
    Fault(CommitStep::kEditorWritten, fail_after);
    Json manifest = {{"format", "rhythm.project"},
                     {"manifest_version",
                      snapshot.soundtrack_ ? (snapshot.soundtrack_->clips_.empty() ? 2 : 3) : 1},
                     {"revision_id", revision},
                     {"project_id", snapshot.document_.id_},
                     {"graph_revision", snapshot.document_.revision_},
                     {"title", snapshot.title_},
                     {"assets", assets},
                     {"graph_sha256", Digest(graph_bytes)},
                     {"editor_sha256", Digest(editor_bytes)}};
    if (snapshot.soundtrack_) manifest["soundtrack"] = soundtrack;
    storage::WriteDurable(directory / "manifest.json", manifest.dump(2));
    Fault(CommitStep::kManifestWritten, fail_after);
    const auto reopened = LoadRevision(directory);
    if (!reopened.warnings_.empty()) throw std::runtime_error("project.reopen_validation");
    Fault(CommitStep::kValidated, fail_after);
    // Persist revision directory entries before publishing the pointer to them.
    storage::SyncDirectory(directory);
    storage::SyncDirectory(directory.parent_path());
    storage::SyncDirectory(project);
    storage::SyncDirectory(std::filesystem::absolute(project).parent_path());
    const auto next = project / ("CURRENT-" + revision);
    storage::WriteDurable(next, revision);
    Fault(CommitStep::kBeforeCommit, fail_after);
    storage::Replace(next, project / "CURRENT");
    Fault(CommitStep::kCommitted, fail_after);
}
}  // namespace rhythm::project
