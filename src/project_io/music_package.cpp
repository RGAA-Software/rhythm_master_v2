#include <array>
#include <atomic>
#include <chrono>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

#include "file_archive.h"
#include "rhythm/project/package.h"
#include "rhythm/storage/atomic_file.h"
#include "soundtrack_codec.h"

namespace rhythm::project {
namespace {
class MusicStaging final {
   public:
    explicit MusicStaging(const std::filesystem::path& parent) {
        static std::atomic<std::uint64_t> sequence{0};
        path_ = parent /
                (".music-package-" +
                 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" +
                 std::to_string(sequence++) + ".tmp");
        if (std::filesystem::exists(path_)) throw std::runtime_error("package.staging_exists");
    }
    ~MusicStaging() {
        std::error_code error;
        std::filesystem::remove(path_, error);
    }
    MusicStaging(const MusicStaging&) = delete;
    MusicStaging& operator=(const MusicStaging&) = delete;
    const std::filesystem::path& Path() const { return path_; }

   private:
    std::filesystem::path path_{};
};
}  // namespace
void InstallPackageFile(const std::filesystem::path& path, storage::FileBytes source,
                        bool fail_before_commit, std::stop_token stop) {
    ReadPackage(source, stop);
    const auto parent =
            path.parent_path().empty() ? std::filesystem::path(".") : path.parent_path();
    std::filesystem::create_directories(parent);
    storage::WriteGuard writer(parent);
    MusicStaging staging(parent);
    {
        std::ofstream output(staging.Path(), std::ios::binary | std::ios::trunc);
        output.exceptions(std::ios::badbit | std::ios::failbit);
        std::array<std::uint8_t, 65536> buffer{};
        std::uint64_t offset = 0;
        while (offset < source.Size()) {
            if (stop.stop_requested()) throw std::runtime_error("package.cancelled");
            const auto count = source.Read(offset, buffer);
            if (!count) throw std::runtime_error("package.source_changed");
            output.write(reinterpret_cast<const char*>(buffer.data()),
                         static_cast<std::streamsize>(count));
            offset += count;
        }
        output.flush();
    }
    storage::SyncFile(staging.Path());
    LoadPackage(staging.Path(), stop);
    if (stop.stop_requested()) throw std::runtime_error("package.cancelled");
    if (fail_before_commit) throw std::runtime_error("package.injected_failure");
    storage::Replace(staging.Path(), path);
    storage::SyncDirectory(parent);
}
void PublishMusicPackage(const std::filesystem::path& path, const graph::Document& document,
                         std::string_view title, std::span<const PackagedAsset> assets,
                         const RuntimePackage::StreamedAudio& audio,
                         const media::Soundtrack& soundtrack, bool fail_before_commit,
                         std::stop_token stop) {
    if (stop.stop_requested()) throw std::runtime_error("package.cancelled");
    if (!soundtrack.clips_.empty() || !audio.bytes_.Valid() ||
        audio.bytes_.Size() != audio.record_.bytes_ ||
        audio.bytes_.Size() > kMaximumMusicAssetBytes ||
        !assets::ValidMediaType(audio.record_.media_type_) ||
        !media::ValidSoundtrack(soundtrack, std::span(&audio.record_, 1)) ||
        assets.size() >= kMaximumPackageAssets ||
        std::any_of(assets.begin(), assets.end(),
                    [&](const auto& item) { return item.record_.id_ == audio.record_.id_; }))
        throw std::invalid_argument("package.media_record");
    // Reuse the small profile's compiler, ordinary asset hash/reference/budget
    // checks and metadata encoding, then add only the explicit file attachment.
    auto entries = detail::ReadArchive(EncodePackage(document, title, assets));
    auto manifest = nlohmann::json::parse(entries.at("manifest.json"));
    manifest["profile"] = "music-performance-v2";
    manifest["streamed_audio"] = {{"sha256", audio.record_.id_.sha256_},
                                  {"bytes", audio.record_.bytes_},
                                  {"media_type", audio.record_.media_type_}};
    manifest["soundtrack"] = detail::EncodeSoundtrack(soundtrack, std::span(&audio.record_, 1));
    entries["manifest.json"] = manifest.dump(2);
    const auto parent =
            path.parent_path().empty() ? std::filesystem::path(".") : path.parent_path();
    std::filesystem::create_directories(parent);
    storage::WriteGuard writer(parent);
    MusicStaging staging(parent);
    detail::WriteFileArchive(
            staging.Path(), entries,
            {"media/" + audio.record_.id_.sha256_, audio.bytes_, audio.record_.id_.sha256_}, stop);
    storage::SyncFile(staging.Path());
    // CRC, media SHA-256, binding and program validation finish before commit.
    LoadPackage(staging.Path(), stop);
    if (stop.stop_requested()) throw std::runtime_error("package.cancelled");
    if (fail_before_commit) throw std::runtime_error("package.injected_failure");
    storage::Replace(staging.Path(), path);
    storage::SyncDirectory(parent);
}
}  // namespace rhythm::project
