#include "rhythm/assets/store.h"

#include <picosha2.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <fstream>
#include <stdexcept>

#include "rhythm/storage/atomic_file.h"
#include "rhythm/storage/file_bytes.h"

namespace rhythm::assets {
namespace {
class StagingFile final {
   public:
    explicit StagingFile(std::filesystem::path path) : path_(std::move(path)) {}
    ~StagingFile() {
        std::error_code error;
        std::filesystem::remove(path_, error);
    }
    const std::filesystem::path& Path() const { return path_; }
    StagingFile(const StagingFile&) = delete;
    StagingFile& operator=(const StagingFile&) = delete;

   private:
    std::filesystem::path path_{};
};
std::string HashFile(const storage::FileBytes& source, std::stop_token cancellation = {}) {
    picosha2::hash256_one_by_one hash;
    std::array<std::uint8_t, 64 * 1024> buffer{};
    std::uint64_t bytes = 0;
    while (bytes < source.Size()) {
        if (cancellation.stop_requested()) throw std::runtime_error("asset.cancelled");
        const auto count = source.Read(bytes, buffer);
        if (!count) throw std::runtime_error("asset.read");
        hash.process(buffer.begin(), buffer.begin() + count);
        bytes += count;
    }
    hash.finish();
    return picosha2::get_hash_hex_string(hash);
}
}  // namespace
bool VerifyFile(const AssetRecord& asset, const storage::FileBytes& bytes,
                std::stop_token cancellation) {
    if (cancellation.stop_requested()) throw std::runtime_error("asset.cancelled");
    return ValidId(asset.id_) && ValidMediaType(asset.media_type_) && bytes.Valid() &&
           bytes.Size() == asset.bytes_ && HashFile(bytes, cancellation) == asset.id_.sha256_;
}
Store::Store(std::filesystem::path directory) : directory_(std::move(directory)) {
    std::filesystem::create_directories(directory_);
    directory_ = std::filesystem::canonical(directory_);
}
std::filesystem::path Store::BlobPath(const AssetId& id) const {
    if (!ValidId(id)) throw std::invalid_argument("asset.id");
    const auto base = directory_ / "sha256";
    const auto shard = base / id.sha256_.substr(0, 2);
    const auto path = shard / id.sha256_;
    for (const auto& entry : {base, shard, path})
        if (std::filesystem::is_symlink(entry)) throw std::invalid_argument("asset.symlink");
    return path;
}
AssetRecord Store::Import(const std::filesystem::path& source, std::string media_type,
                          std::uint64_t maximum_bytes, std::stop_token cancellation) {
    return ImportChecked(source, std::move(media_type), maximum_bytes, cancellation, {});
}
void Store::Restore(const std::filesystem::path& source, const AssetRecord& expected,
                    std::stop_token cancellation) {
    if (!ValidId(expected.id_) || !ValidMediaType(expected.media_type_))
        throw std::invalid_argument("asset.repair_record");
    (void)ImportChecked(source, expected.media_type_, expected.bytes_, cancellation, expected);
}
AssetHealth Store::Inspect(const AssetRecord& asset, std::stop_token cancellation) const {
    if (cancellation.stop_requested()) throw std::runtime_error("asset.cancelled");
    try {
        const auto path = BlobPath(asset.id_);
        if (!std::filesystem::exists(path)) return AssetHealth::kMissing;
        (void)Open(asset, 1024ULL * 1024 * 1024, cancellation);
        return AssetHealth::kValid;
    } catch (const std::exception&) {
        if (cancellation.stop_requested()) throw std::runtime_error("asset.cancelled");
        return AssetHealth::kCorrupt;
    }
}
AssetRecord Store::ImportChecked(const std::filesystem::path& source, std::string media_type,
                                 std::uint64_t maximum_bytes, std::stop_token cancellation,
                                 const std::optional<AssetRecord>& expected) {
    if (!ValidMediaType(media_type)) throw std::invalid_argument("asset.media_type");
    if (maximum_bytes > 1024ull * 1024 * 1024) throw std::length_error("asset.import_budget");
    const auto expected_bytes = std::filesystem::file_size(source);
    if (expected_bytes > maximum_bytes) throw std::length_error("asset.byte_limit");
    if (cancellation.stop_requested()) throw std::runtime_error("asset.cancelled");
    storage::WriteGuard writer(directory_);
    static std::atomic<std::uint64_t> sequence{0};
    const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto staging = directory_ / (".import-" + std::to_string(tick) + "-" +
                                       std::to_string(sequence++) + ".tmp");
    if (std::filesystem::exists(staging)) throw std::runtime_error("asset.staging_exists");
    StagingFile pending(staging);
    std::ifstream input(source, std::ios::binary);
    if (!input) throw std::runtime_error("asset.open");
    picosha2::hash256_one_by_one hash;
    std::uint64_t bytes = 0;
    {
        std::ofstream output(pending.Path(), std::ios::binary | std::ios::trunc);
        output.exceptions(std::ios::badbit | std::ios::failbit);
        std::array<char, 64 * 1024> buffer{};
        while (input.read(buffer.data(), buffer.size()) || input.gcount()) {
            if (cancellation.stop_requested()) throw std::runtime_error("asset.cancelled");
            bytes += static_cast<std::uint64_t>(input.gcount());
            if (bytes > maximum_bytes || bytes > expected_bytes)
                throw std::length_error("asset.byte_limit");
            output.write(buffer.data(), input.gcount());
            hash.process(buffer.begin(), buffer.begin() + input.gcount());
        }
        output.flush();
    }
    if (!input.eof() || bytes != expected_bytes) throw std::runtime_error("asset.source_changed");
    hash.finish();
    const AssetRecord asset{{picosha2::get_hash_hex_string(hash)}, bytes, std::move(media_type)};
    if (expected && asset != *expected) throw std::runtime_error("asset.repair_mismatch");
    const auto destination = BlobPath(asset.id_);
    if (std::filesystem::exists(destination)) {
        if (Verify(asset)) return asset;
        if (!expected) throw std::runtime_error("asset.existing_corrupt");
    }
    storage::SyncFile(pending.Path());
    if (HashFile(storage::FileBytes::Open(pending.Path(), bytes), cancellation) !=
        asset.id_.sha256_)
        throw std::runtime_error("asset.staging_hash");
    if (cancellation.stop_requested()) throw std::runtime_error("asset.cancelled");
    std::filesystem::create_directories(destination.parent_path());
    storage::Replace(pending.Path(), destination);
    storage::SyncDirectory(destination.parent_path());
    storage::SyncDirectory(destination.parent_path().parent_path());
    storage::SyncDirectory(directory_);
    return asset;
}
bool Store::Verify(const AssetRecord& asset) const {
    try {
        const auto path = BlobPath(asset.id_);
        return asset.bytes_ <= 1024ull * 1024 * 1024 &&
               std::filesystem::file_size(path) == asset.bytes_ &&
               HashFile(storage::FileBytes::Open(path, asset.bytes_)) == asset.id_.sha256_;
    } catch (const std::exception&) {
        return false;
    }
}
storage::FileBytes Store::Open(const AssetRecord& asset, std::uint64_t maximum_bytes,
                               std::stop_token cancellation) const {
    if (cancellation.stop_requested()) throw std::runtime_error("asset.cancelled");
    if (asset.bytes_ > maximum_bytes || maximum_bytes > 1024ull * 1024 * 1024)
        throw std::length_error("asset.read_budget");
    auto bytes = storage::FileBytes::Open(BlobPath(asset.id_), asset.bytes_);
    if (!VerifyFile(asset, bytes, cancellation)) throw std::runtime_error("asset.hash");
    return bytes;
}
AssetRecord Store::CopyFrom(const Store& source, const AssetRecord& asset,
                            std::uint64_t maximum_bytes, std::stop_token cancellation) {
    if (asset.bytes_ > maximum_bytes) throw std::length_error("asset.byte_limit");
    if (!source.Verify(asset)) throw std::invalid_argument("asset.source_integrity");
    const auto copied =
            Import(source.BlobPath(asset.id_), asset.media_type_, maximum_bytes, cancellation);
    if (copied != asset) throw std::invalid_argument("asset.source_changed");
    return copied;
}
std::string Store::Read(const AssetRecord& asset, std::uint64_t maximum_bytes) const {
    if (asset.bytes_ > maximum_bytes || maximum_bytes > 1024ull * 1024 * 1024)
        throw std::length_error("asset.read_budget");
    const auto path = BlobPath(asset.id_);
    if (std::filesystem::file_size(path) != asset.bytes_)
        throw std::runtime_error("asset.size_changed");
    std::ifstream input(path, std::ios::binary);
    input.exceptions(std::ios::failbit | std::ios::badbit);
    std::string bytes(static_cast<std::size_t>(asset.bytes_), '\0');
    input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (picosha2::hash256_hex_string(bytes.begin(), bytes.end()) != asset.id_.sha256_)
        throw std::runtime_error("asset.hash");
    return bytes;
}
}  // namespace rhythm::assets
