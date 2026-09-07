#include "file_archive.h"

#include <picosha2.h>

#include <array>
#include <ctime>
#include <fstream>
#include <memory>
#include <stdexcept>

#include "archive_memory.h"
#include "rhythm/project/package.h"

namespace rhythm::project::detail {
namespace {
void CheckStop(std::stop_token stop) {
    if (stop.stop_requested()) throw std::runtime_error("package.cancelled");
}
bool MediaName(const std::string& name) {
    return name.starts_with("media/") && assets::ValidId({name.substr(6)});
}
class FileReader final {
   public:
    FileReader(storage::FileBytes source, std::stop_token stop)
        : source_(std::move(source)), stop_(stop) {
        CheckStop(stop_);
        memory_.Bind(archive_);
        archive_.m_pIO_opaque = this;
        archive_.m_pRead = Read;
        if (!mz_zip_reader_init(&archive_, source_.Size(), 0))
            throw std::invalid_argument("package.zip");
    }
    ~FileReader() { mz_zip_reader_end(&archive_); }
    FileReader(const FileReader&) = delete;
    FileReader& operator=(const FileReader&) = delete;
    mz_zip_archive& Archive() { return archive_; }
    std::size_t PeakBytes() const { return memory_.PeakBytes(); }

   private:
    static std::size_t Read(void* opaque, mz_uint64 offset, void* buffer,
                            std::size_t count) noexcept {
        auto& reader = *static_cast<FileReader*>(opaque);
        if (reader.stop_.stop_requested()) return 0;
        try {
            return reader.source_.Read(offset, {static_cast<std::uint8_t*>(buffer), count});
        } catch (...) {
            return 0;
        }
    }
    storage::FileBytes source_{};
    std::stop_token stop_{};
    ArchiveMemory memory_{};
    mz_zip_archive archive_{};
};
struct IteratorClose {
    void operator()(mz_zip_reader_extract_iter_state* state) const {
        mz_zip_reader_extract_iter_free(state);
    }
};
ArchiveMedia ReadMedia(mz_zip_archive& archive, mz_uint index, const std::string& name,
                       const storage::FileBytes& source, std::stop_token stop) {
    std::unique_ptr<mz_zip_reader_extract_iter_state, IteratorClose> iterator(
            mz_zip_reader_extract_iter_new(&archive, index, 0));
    if (!iterator) throw std::invalid_argument("package.media_entry");
    // The pinned miniz iterator has already validated the local header and
    // computed its payload offset. STORE requires no decompression/extra copy.
    const auto range = source.Slice(iterator->cur_file_ofs, iterator->file_stat.m_uncomp_size);
    picosha2::hash256_one_by_one hash;
    std::array<std::uint8_t, 65536> buffer{};
    std::uint64_t read = 0;
    while (read < range.Size()) {
        CheckStop(stop);
        const auto count =
                mz_zip_reader_extract_iter_read(iterator.get(), buffer.data(), buffer.size());
        if (!count || count > range.Size() - read)
            throw std::invalid_argument("package.media_read");
        hash.process(buffer.begin(), buffer.begin() + count);
        read += count;
    }
    // Finalization verifies complete size and CRC, and releases iterator state.
    if (!mz_zip_reader_extract_iter_free(iterator.release()))
        throw std::invalid_argument("package.entry_crc");
    hash.finish();
    const auto digest = picosha2::get_hash_hex_string(hash);
    if (digest != name.substr(6)) throw std::invalid_argument("package.media_hash");
    return {name, range, digest};
}
struct MediaInput {
    storage::FileBytes bytes_{};
    std::stop_token stop_{};
    static std::size_t Read(void* opaque, mz_uint64 offset, void* buffer,
                            std::size_t count) noexcept {
        auto& input = *static_cast<MediaInput*>(opaque);
        if (input.stop_.stop_requested()) return 0;
        try {
            return input.bytes_.Read(offset, {static_cast<std::uint8_t*>(buffer), count});
        } catch (...) {
            return 0;
        }
    }
};
class FileWriter final {
   public:
    FileWriter(const std::filesystem::path& path, std::stop_token stop)
        : output_(path, std::ios::binary | std::ios::trunc), stop_(stop) {
        if (!output_) throw std::runtime_error("package.staging_open");
        memory_.Bind(archive_);
        archive_.m_pIO_opaque = this;
        archive_.m_pWrite = Write;
        if (!mz_zip_writer_init(&archive_, 0)) throw std::runtime_error("package.zip_writer");
    }
    ~FileWriter() { mz_zip_writer_end(&archive_); }
    FileWriter(const FileWriter&) = delete;
    FileWriter& operator=(const FileWriter&) = delete;
    mz_zip_archive& Archive() { return archive_; }
    void Finish() {
        CheckStop(stop_);
        if (!mz_zip_writer_finalize_archive(&archive_))
            throw std::runtime_error("package.zip_finalize");
        output_.flush();
        if (!output_) throw std::runtime_error("package.staging_flush");
    }

   private:
    static std::size_t Write(void* opaque, mz_uint64 offset, const void* buffer,
                             std::size_t count) noexcept {
        auto& writer = *static_cast<FileWriter*>(opaque);
        if (writer.stop_.stop_requested() || offset > kMaximumFileArchiveBytes ||
            count > kMaximumFileArchiveBytes - offset)
            return 0;
        try {
            writer.output_.seekp(static_cast<std::streamoff>(offset));
            writer.output_.write(static_cast<const char*>(buffer),
                                 static_cast<std::streamsize>(count));
            return writer.output_ ? count : 0;
        } catch (...) {
            return 0;
        }
    }
    std::ofstream output_{};
    std::stop_token stop_{};
    ArchiveMemory memory_{};
    mz_zip_archive archive_{};
};
}  // namespace

FileArchive ReadFileArchive(storage::FileBytes source, std::stop_token stop) {
    if (!source.Valid() || !source.Size() || source.Size() > kMaximumFileArchiveBytes)
        throw std::length_error("package.archive_bytes");
    FileReader reader(source, stop);
    auto& archive = reader.Archive();
    const auto count = mz_zip_reader_get_num_files(&archive);
    if (count < 2 || count > kMaximumPackageAssets + 3 || archive.m_archive_size != source.Size() ||
        mz_zip_get_archive_file_start_offset(&archive) != 0)
        throw std::invalid_argument("package.entry_count_or_prefix");
    FileArchive result;
    std::uint64_t asset_bytes = 0;
    for (mz_uint index = 0; index < count; ++index) {
        CheckStop(stop);
        mz_zip_archive_file_stat stat{};
        if (!mz_zip_reader_file_stat(&archive, index, &stat))
            throw std::invalid_argument("package.entry");
        const std::string name(stat.m_filename);
        const bool media = MediaName(name);
        const bool asset = name.starts_with("assets/") && assets::ValidId({name.substr(7)});
        if ((!media && !asset && name != "manifest.json" && name != "runtime/program.pb") ||
            result.entries_.contains(name) || (media && result.media_) ||
            mz_zip_reader_get_filename(&archive, index, nullptr, 0) != name.size() + 1 ||
            stat.m_is_directory || stat.m_is_encrypted ||
            ((stat.m_external_attr >> 16) & 0170000) == 0120000)
            throw std::invalid_argument("package.entry_name");
        if (media) {
            if (!stat.m_uncomp_size || stat.m_uncomp_size > kMaximumStreamedMusicBytes ||
                stat.m_method != 0 || stat.m_comp_size != stat.m_uncomp_size)
                throw std::invalid_argument("package.media_profile");
            result.media_ = ReadMedia(archive, index, name, source, stop);
            continue;
        }
        const auto maximum = name == "manifest.json" ? std::size_t{65536}
                             : asset                 ? kMaximumPackageAssetBytes
                                                     : kMaximumProgramBytes;
        if (stat.m_uncomp_size > maximum) throw std::length_error("package.entry_bytes");
        if (asset) {
            if (stat.m_uncomp_size > kMaximumPackageAssetBytes - asset_bytes)
                throw std::length_error("package.asset_bytes");
            asset_bytes += stat.m_uncomp_size;
        }
        std::string bytes(static_cast<std::size_t>(stat.m_uncomp_size), '\0');
        if (!mz_zip_reader_extract_to_mem(&archive, index, bytes.data(), bytes.size(), 0))
            throw std::invalid_argument("package.entry_crc");
        result.entries_.emplace(name, std::move(bytes));
    }
    result.native_peak_bytes_ = reader.PeakBytes();
    return result;
}
void WriteFileArchive(const std::filesystem::path& path, const PackageEntries& entries,
                      const ArchiveMedia& media, std::stop_token stop) {
    CheckStop(stop);
    if (!MediaName(media.name_) || !media.bytes_.Valid() || !media.bytes_.Size() ||
        media.bytes_.Size() > kMaximumStreamedMusicBytes ||
        entries.size() > kMaximumPackageAssets + 2)
        throw std::invalid_argument("package.media_profile");
    // Keep ordinary input bounded without buffering the music. Full entry-name,
    // manifest and checksum validation runs before the caller's atomic commit.
    std::uint64_t ordinary_bytes = 0;
    for (const auto& [name, bytes] : entries) {
        if (bytes.size() > kMaximumPackageBytes - ordinary_bytes)
            throw std::length_error("package.entry_bytes");
        ordinary_bytes += bytes.size();
        if (name == media.name_) throw std::invalid_argument("package.entry_name");
    }
    FileWriter writer(path, stop);
    std::tm date{};
    date.tm_year = 80;
    date.tm_mday = 1;
    auto timestamp = std::mktime(&date);
    for (const auto& [name, bytes] : entries) {
        CheckStop(stop);
        if (!mz_zip_writer_add_mem_ex_v2(&writer.Archive(), name.c_str(), bytes.data(),
                                         bytes.size(), nullptr, 0, MZ_BEST_COMPRESSION, 0, 0,
                                         &timestamp, nullptr, 0, nullptr, 0))
            throw std::runtime_error("package.zip_write");
    }
    MediaInput input{media.bytes_, stop};
    if (!mz_zip_writer_add_read_buf_callback(&writer.Archive(), media.name_.c_str(),
                                             MediaInput::Read, &input, media.bytes_.Size(),
                                             &timestamp, nullptr, 0, 0, nullptr, 0, nullptr, 0))
        throw std::runtime_error("package.media_write");
    writer.Finish();
}
}  // namespace rhythm::project::detail
