#include <picosha2.h>

#include <atomic>
#include <chrono>
#include <fstream>
#include <nlohmann/json.hpp>
#include <set>
#include <stdexcept>

#include "package_archive.h"
#include "rhythm/project/package.h"
#include "rhythm/storage/atomic_file.h"
#include "soundtrack_codec.h"

namespace rhythm::project {
namespace {
using Json = nlohmann::json;
std::string Hash(std::string_view bytes) {
    return picosha2::hash256_hex_string(bytes.begin(), bytes.end());
}
Json Operators(const graph::ExecutionPlan& plan) {
    std::set<std::string> types;
    for (const auto& instruction : plan.instructions_) types.insert(instruction.node_.type_);
    return types;
}
void ValidateReferences(const graph::ExecutionPlan& plan, std::span<const PackagedAsset> assets) {
    std::set<std::string> available;
    for (const auto& asset : assets) available.insert(asset.record_.id_.sha256_);
    for (const auto& instruction : plan.instructions_)
        for (const auto& [key, property] : instruction.node_.properties_) {
            if (!std::holds_alternative<assets::AssetId>(property)) continue;
            const auto& id = std::get<assets::AssetId>(property);
            if (!id.sha256_.empty() && !available.contains(id.sha256_))
                throw std::invalid_argument("package.asset_missing");
        }
}
class StagedPackage final {
   public:
    explicit StagedPackage(std::filesystem::path path) : path_(std::move(path)) {}
    ~StagedPackage() {
        std::error_code error;
        std::filesystem::remove(path_, error);
    }
    StagedPackage(const StagedPackage&) = delete;
    StagedPackage& operator=(const StagedPackage&) = delete;
    const std::filesystem::path& Path() const { return path_; }

   private:
    std::filesystem::path path_{};
};
}  // namespace

std::string EncodePackage(const graph::Document& document, std::string_view title,
                          std::span<const PackagedAsset> assets,
                          const std::optional<media::Soundtrack>& soundtrack) {
    if (title.size() > 4096) throw std::length_error("package.title");
    if (assets.size() > kMaximumPackageAssets) throw std::length_error("package.asset_count");
    detail::PackageEntries entries;
    Json asset_records = Json::array();
    std::vector<assets::AssetRecord> records;
    std::size_t asset_bytes = 0;
    for (const auto& asset : assets) {
        if (!assets::ValidId(asset.record_.id_) ||
            !assets::ValidMediaType(asset.record_.media_type_) ||
            asset.record_.bytes_ != asset.bytes_.size())
            throw std::invalid_argument("package.asset_record");
        if (asset.bytes_.size() > kMaximumPackageAssetBytes - asset_bytes)
            throw std::length_error("package.asset_bytes");
        asset_bytes += asset.bytes_.size();
        records.push_back(asset.record_);
        if (Hash(asset.bytes_) != asset.record_.id_.sha256_ ||
            !entries.emplace("assets/" + asset.record_.id_.sha256_, asset.bytes_).second)
            throw std::invalid_argument("package.asset_hash");
        asset_records.push_back({{"sha256", asset.record_.id_.sha256_},
                                 {"bytes", asset.record_.bytes_},
                                 {"media_type", asset.record_.media_type_}});
    }
    const auto compiled = graph::Compile(document, graph::Registry{});
    if (!std::holds_alternative<graph::ExecutionPlan>(compiled))
        throw std::invalid_argument("package.invalid_program");
    const auto& plan = std::get<graph::ExecutionPlan>(compiled);
    ValidateReferences(plan, assets);
    const auto program = EncodeProgram(plan);
    Json manifest = {{"format", "rhythm.runtime"},
                     {"manifest_version", 1},
                     {"program_abi", 2},
                     {"profile", soundtrack ? "music-performance-v1" : "texture-signal-v2"},
                     {"canvas", {{"width", plan.canvas_.width_}, {"height", plan.canvas_.height_}}},
                     {"document_id", plan.document_id_},
                     {"revision", plan.revision_},
                     {"title", title},
                     {"program_sha256", Hash(program)},
                     {"program_bytes", program.size()},
                     {"operators", Operators(plan)}};
    manifest["assets"] = std::move(asset_records);
    if (soundtrack) manifest["soundtrack"] = detail::EncodeSoundtrack(*soundtrack, records);
    entries.emplace("manifest.json", manifest.dump(2));
    entries.emplace("runtime/program.pb", program);
    return detail::WriteArchive(entries);
}

RuntimePackage DecodePackage(std::string_view bytes) {
    const auto entries = detail::ReadArchive(bytes);
    std::vector<std::set<std::string>> object_keys;
    const auto manifest = Json::parse(
            entries.at("manifest.json"), [&](int depth, Json::parse_event_t event, Json& value) {
                if (depth > 16) throw std::length_error("package.metadata_depth");
                if (event == Json::parse_event_t::object_start) object_keys.emplace_back();
                if (event == Json::parse_event_t::key &&
                    !object_keys.back().insert(value.get<std::string>()).second)
                    throw std::invalid_argument("package.metadata_duplicate_key");
                if (event == Json::parse_event_t::object_end) object_keys.pop_back();
                return true;
            });
    const bool music = manifest.at("profile") == "music-performance-v1";
    const bool current = music || manifest.at("profile") == "texture-signal-v2";
    if (manifest.at("format") != "rhythm.runtime" || manifest.at("manifest_version") != 1 ||
        manifest.at("program_abi") != (current ? 2 : 1) ||
        (!current && manifest.at("profile") != "texture-signal-v1" &&
         manifest.at("profile") != "texture-signal-assets-v1"))
        throw std::invalid_argument("package.profile");
    const auto& program = entries.at("runtime/program.pb");
    if (manifest.at("program_bytes") != program.size() ||
        manifest.at("program_sha256") != Hash(program))
        throw std::invalid_argument("package.hash");
    RuntimePackage package;
    package.profile_ = music     ? PackageProfile::kMusicPerformanceV1
                       : current ? PackageProfile::kTextureSignalV2
                       : manifest.at("profile") == "texture-signal-assets-v1"
                               ? PackageProfile::kTextureSignalAssetsV1
                               : PackageProfile::kTextureSignalV1;
    if (current || manifest.at("profile") == "texture-signal-assets-v1") {
        const auto& records = manifest.at("assets");
        if (!records.is_array() || (!current && records.empty()) ||
            records.size() > kMaximumPackageAssets)
            throw std::invalid_argument("package.asset_count");
        std::set<std::string> seen;
        for (const auto& record : records) {
            const assets::AssetRecord asset{{record.at("sha256").get<std::string>()},
                                            record.at("bytes").get<std::uint64_t>(),
                                            record.at("media_type").get<std::string>()};
            if (!assets::ValidId(asset.id_) || !assets::ValidMediaType(asset.media_type_) ||
                !record.at("bytes").is_number_unsigned() || !seen.insert(asset.id_.sha256_).second)
                throw std::invalid_argument("package.asset_record");
            const auto& content = entries.at("assets/" + asset.id_.sha256_);
            if (content.size() != asset.bytes_ || Hash(content) != asset.id_.sha256_)
                throw std::invalid_argument("package.asset_hash");
            package.assets_.push_back({asset, content});
        }
    } else if (manifest.contains("assets")) {
        throw std::invalid_argument("package.profile");
    }
    if (music) {
        std::vector<assets::AssetRecord> records;
        for (const auto& asset : package.assets_) records.push_back(asset.record_);
        package.soundtrack_ = detail::DecodeSoundtrack(manifest.at("soundtrack"), records);
    } else if (manifest.contains("soundtrack")) {
        throw std::invalid_argument("package.soundtrack_profile");
    }
    if (entries.size() != package.assets_.size() + 2)
        throw std::invalid_argument("package.unlisted_asset");
    package.program_ = DecodeProgram(program, current ? 2 : 1);
    ValidateReferences(package.program_, package.assets_);
    if (current) {
        if (manifest.at("canvas").at("width") != package.program_.canvas_.width_ ||
            manifest.at("canvas").at("height") != package.program_.canvas_.height_)
            throw std::invalid_argument("package.canvas");
    } else if (manifest.contains("canvas")) {
        throw std::invalid_argument("package.canvas_abi");
    }
    package.title_ = manifest.at("title").get<std::string>();
    if (package.title_.size() > 4096 ||
        manifest.at("document_id") != package.program_.document_id_ ||
        manifest.at("revision") != package.program_.revision_ ||
        manifest.at("operators") != Operators(package.program_))
        throw std::invalid_argument("package.identity");
    return package;
}

RuntimePackage LoadPackage(const std::filesystem::path& path) {
    const auto size = std::filesystem::file_size(path);
    if (size > kMaximumPackageBytes) throw std::length_error("package.archive_bytes");
    std::ifstream file(path, std::ios::binary);
    file.exceptions(std::ios::badbit | std::ios::failbit);
    std::string bytes(static_cast<std::size_t>(size), '\0');
    file.read(bytes.data(), static_cast<std::streamsize>(size));
    return DecodePackage(bytes);
}

void PublishPackage(const std::filesystem::path& path, const graph::Document& document,
                    std::string_view title, bool fail_before_commit) {
    const auto bytes = EncodePackage(document, title);
    InstallPackage(path, bytes, fail_before_commit);
}
void InstallPackage(const std::filesystem::path& path, std::string_view bytes,
                    bool fail_before_commit) {
    DecodePackage(bytes);
    const auto parent =
            path.parent_path().empty() ? std::filesystem::path(".") : path.parent_path();
    std::filesystem::create_directories(parent);
    storage::WriteGuard writer(parent);
    static std::atomic<std::uint64_t> sequence{0};
    const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto staging = parent / (".package-" + std::to_string(tick) + "-" +
                                   std::to_string(sequence++) + ".tmp");
    if (std::filesystem::exists(staging)) throw std::runtime_error("package.staging_exists");
    StagedPackage pending(staging);
    storage::WriteDurable(pending.Path(), bytes);
    LoadPackage(pending.Path());
    if (fail_before_commit) throw std::runtime_error("package.injected_failure");
    storage::Replace(pending.Path(), path);
    storage::SyncDirectory(parent);
}
}  // namespace rhythm::project
