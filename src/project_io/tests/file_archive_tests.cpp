#include <picosha2.h>

#include <array>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>

#include "file_archive.h"
#include "rhythm/storage/atomic_file.h"

namespace {
void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
template <typename Action>
void Reject(Action action) {
    bool rejected = false;
    try {
        action();
    } catch (const std::exception&) {
        rejected = true;
    }
    Check(rejected, "invalid file archive accepted");
}
void Run(const std::filesystem::path& directory) {
    using namespace rhythm;
    namespace detail = project::detail;
    std::filesystem::create_directories(directory);
    const auto source = directory / "source.bin";
    picosha2::hash256_one_by_one hash;
    {
        std::ofstream output(source, std::ios::binary | std::ios::trunc);
        output.exceptions(std::ios::badbit | std::ios::failbit);
        std::array<char, 65536> buffer{};
        for (int index = 0; index < 640; ++index) {
            buffer.fill(static_cast<char>(index % 251));
            output.write(buffer.data(), buffer.size());
            hash.process(buffer.begin(), buffer.end());
        }
    }
    hash.finish();
    const auto digest = picosha2::get_hash_hex_string(hash);
    const auto path = directory / std::filesystem::path(u8"完整音乐.rhythmpack");
    const detail::PackageEntries entries{{"manifest.json", "{}"},
                                         {"runtime/program.pb", "program"}};
    const detail::ArchiveMedia media{"media/" + digest,
                                     storage::FileBytes::Open(source, 64 * 1024 * 1024), digest};
    detail::WriteFileArchive(path, entries, media);
    const auto opened = detail::ReadFileArchive(
            storage::FileBytes::Open(path, detail::kMaximumFileArchiveBytes));
    Check(opened.entries_ == entries && opened.media_ && opened.media_->sha256_ == digest &&
                  opened.media_->bytes_.Size() == media.bytes_.Size(),
          "large stored media and metadata round trip");
    Check(opened.native_peak_bytes_ <= 2 * 1024 * 1024, "native ZIP allocation budget");
    std::array<std::uint8_t, 1024> bytes{};
    Check(opened.media_->bytes_.Read(299 * 65536, bytes) == bytes.size() && bytes.front() == 48 &&
                  bytes.back() == 48,
          "returned music range must directly address the stored payload");
    const auto corrupt = directory / "corrupt.rhythmpack";
    std::filesystem::copy_file(path, corrupt, std::filesystem::copy_options::overwrite_existing);
    {
        std::fstream file(corrupt, std::ios::binary | std::ios::in | std::ios::out);
        file.seekp(5 * 1024 * 1024);
        file.put('!');
    }
    Reject([&] {
        detail::ReadFileArchive(
                storage::FileBytes::Open(corrupt, detail::kMaximumFileArchiveBytes));
    });
    std::stop_source stop;
    stop.request_stop();
    Reject([&] {
        detail::ReadFileArchive(storage::FileBytes::Open(path, detail::kMaximumFileArchiveBytes),
                                stop.get_token());
    });
    Reject([&] {
        detail::WriteFileArchive(directory / "cancelled", entries, media, stop.get_token());
    });
    Check(!std::filesystem::exists(directory / "cancelled"),
          "pre-cancelled writer must not create staging output");
    auto bad_name = media;
    bad_name.name_ = "media/../../outside";
    Reject([&] { detail::WriteFileArchive(directory / "bad-name", entries, bad_name); });
    const auto small_path = directory / "small.zip";
    storage::WriteDurable(small_path, detail::WriteArchive(entries));
    const auto small =
            detail::ReadFileArchive(storage::FileBytes::Open(small_path, 16 * 1024 * 1024));
    Check(!small.media_ && small.entries_ == entries,
          "legacy small ZIPs remain readable through file I/O");
    detail::PackageEntries oversized_directory;
    for (int index = 0; index < 7000; ++index)
        oversized_directory.emplace(std::string(300, 'a') + std::to_string(index), "");
    const auto attack = directory / "large-directory.zip";
    storage::WriteDurable(attack, detail::WriteArchive(oversized_directory));
    bool allocator_rejected = false;
    try {
        detail::ReadFileArchive(storage::FileBytes::Open(attack, detail::kMaximumFileArchiveBytes));
    } catch (const std::exception& error) {
        allocator_rejected = std::string(error.what()) == "package.zip";
    }
    Check(allocator_rejected,
          "oversized central directory must fail during bounded native allocation");
    graph::Registry registry;
    graph::Document document;
    document.id_ = "streamed.music.contract";
    document.nodes_ = {registry.MakeNode(1, "texture.gradient"),
                       registry.MakeNode(2, "output.texture")};
    document.edges_ = {{1, 1, 2, "source"}};
    document.output_ = 2;
    const project::RuntimePackage::StreamedAudio audio{
            {{digest}, media.bytes_.Size(), "audio/x-rhythm-media"}, media.bytes_};
    const media::Soundtrack soundtrack{{digest}, "Complete music", 0.5F, true};
    Check(project::RequiresStreamedAudio(std::span(&audio.record_, 1), soundtrack),
          "large bound song selects streamed profile");
    Reject([&] { project::RequiresStreamedAudio(std::span(&audio.record_, 1), {}); });
    auto oversized_music = audio.record_;
    oversized_music.bytes_ = project::kMaximumMusicAssetBytes + 1;
    Reject([&] { project::RequiresStreamedAudio(std::span(&oversized_music, 1), soundtrack); });
    auto ordinary = audio.record_;
    ordinary.id_.sha256_ = std::string(64, 'a');
    ordinary.media_type_ = "image/png";
    ordinary.bytes_ = project::kMaximumPackageAssetBytes;
    const std::array records{audio.record_, ordinary};
    Check(project::RequiresStreamedAudio(records, soundtrack), "independent ordinary asset budget");
    ordinary.bytes_++;
    const std::array overflow{audio.record_, ordinary};
    Reject([&] { project::RequiresStreamedAudio(overflow, soundtrack); });
    const auto performance = directory / "music-profile.rhythmpack";
    project::PublishMusicPackage(performance, document, "Performance", {}, audio, soundtrack);
    const auto package = project::LoadPackage(performance);
    Check(package.profile_ == project::PackageProfile::kMusicPerformanceV2 &&
                  package.streamed_audio_ && package.streamed_audio_->record_ == audio.record_ &&
                  package.assets_.empty() && package.soundtrack_ == soundtrack &&
                  package.program_.document_id_ == document.id_,
          "versioned large music package retains metadata and a file lease instead of a song "
          "buffer");
    Reject([&] {
        project::PublishMusicPackage(performance, document, "Failed", {}, audio, soundtrack, true);
    });
    Check(project::LoadPackage(performance).title_ == "Performance",
          "failed publish preserves package");
    const auto installed = directory / "installed.rhythmpack";
    project::PublishPackage(installed, document, "Installed before import");
    Reject([&] {
        project::InstallPackageFile(
                installed, storage::FileBytes::Open(performance, project::kMaximumFilePackageBytes),
                true);
    });
    Check(project::LoadPackage(installed).title_ == "Installed before import",
          "failed file installation preserves installed package");
    project::PublishMusicPackage(performance, document, "Updated", {}, audio, soundtrack);
    Check(project::LoadPackage(performance).title_ == "Updated" &&
                  package.streamed_audio_->bytes_.Read(299 * 65536, bytes) == bytes.size() &&
                  bytes.front() == 48,
          "atomic large package publication preserves old playback readers");
    const auto authored = detail::ReadFileArchive(
            storage::FileBytes::Open(performance, detail::kMaximumFileArchiveBytes));
    Reject([&] { project::DecodePackage(detail::WriteArchive(authored.entries_)); });
    auto bad_entries = authored.entries_;
    auto metadata = nlohmann::json::parse(bad_entries.at("manifest.json"));
    metadata["streamed_audio"]["bytes"] = 1;
    bad_entries["manifest.json"] = metadata.dump();
    const auto wrong_size = directory / "wrong-size.rhythmpack";
    detail::WriteFileArchive(wrong_size, bad_entries, media);
    Reject([&] { project::LoadPackage(wrong_size); });
    std::cout << "20 MiB music archive: range/hash/CRC, cancellation, old ZIP and metadata "
                 "allocation passed; native_peak="
              << opened.native_peak_bytes_ << '\n';
}
}  // namespace
int main(int argc, char* argv[]) {
    try {
        if (argc != 2) throw std::invalid_argument("file_archive_tests output");
        Run(std::filesystem::path(argv[1]));
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
