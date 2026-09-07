#include "package_archive.h"

#include <miniz.h>

#include <ctime>
#include <memory>
#include <stdexcept>

#include "rhythm/project/package.h"

namespace rhythm::project::detail {
namespace {
class ZipReader final {
   public:
    explicit ZipReader(std::string_view bytes) {
        if (!mz_zip_reader_init_mem(&archive_, bytes.data(), bytes.size(), 0))
            throw std::invalid_argument("package.zip");
    }
    ~ZipReader() { mz_zip_reader_end(&archive_); }
    ZipReader(const ZipReader&) = delete;
    ZipReader& operator=(const ZipReader&) = delete;
    mz_zip_archive& Archive() { return archive_; }

   private:
    // The library owns internal allocations and borrows the enclosing call's
    // immutable bytes. This private synchronous adapter never escapes that call.
    mz_zip_archive archive_{};
};
class ZipWriter final {
   public:
    ZipWriter() {
        if (!mz_zip_writer_init_heap(&archive_, 0, 0))
            throw std::runtime_error("package.zip_writer");
    }
    ~ZipWriter() { mz_zip_writer_end(&archive_); }
    ZipWriter(const ZipWriter&) = delete;
    ZipWriter& operator=(const ZipWriter&) = delete;
    mz_zip_archive& Archive() { return archive_; }

   private:
    mz_zip_archive archive_{};
};
struct BufferDeleter {
    void operator()(void* buffer) const { mz_free(buffer); }
};
}  // namespace

std::string WriteArchive(const PackageEntries& entries) {
    ZipWriter writer;
    std::tm date{};
    date.tm_year = 80;
    date.tm_mday = 1;
    auto timestamp = std::mktime(&date);  // ZIP stores local time; fix its calendar date.
    for (const auto& [name, bytes] : entries)
        if (!mz_zip_writer_add_mem_ex_v2(&writer.Archive(), name.c_str(), bytes.data(),
                                         bytes.size(), nullptr, 0, MZ_BEST_COMPRESSION, 0, 0,
                                         &timestamp, nullptr, 0, nullptr, 0))
            throw std::runtime_error("package.zip_write");
    void* data = nullptr;
    std::size_t size = 0;
    if (!mz_zip_writer_finalize_heap_archive(&writer.Archive(), &data, &size))
        throw std::runtime_error("package.zip_finalize");
    const std::unique_ptr<void, BufferDeleter> owned(data);
    if (!owned || size > kMaximumPackageBytes) throw std::length_error("package.archive_bytes");
    return {static_cast<const char*>(owned.get()), size};
}

PackageEntries ReadArchive(std::string_view bytes) {
    if (bytes.size() > kMaximumPackageBytes) throw std::length_error("package.archive_bytes");
    ZipReader reader(bytes);
    auto& archive = reader.Archive();
    const auto count = mz_zip_reader_get_num_files(&archive);
    if (count < 2 || count > kMaximumPackageAssets + 2)
        throw std::invalid_argument("package.entry_count");
    PackageEntries entries;
    std::uint64_t asset_bytes = 0;
    for (mz_uint index = 0; index < count; ++index) {
        mz_zip_archive_file_stat stat{};
        if (!mz_zip_reader_file_stat(&archive, index, &stat))
            throw std::invalid_argument("package.entry");
        const std::string name(stat.m_filename);
        const bool asset = name.starts_with("assets/") && assets::ValidId({name.substr(7)});
        if ((name != "manifest.json" && name != "runtime/program.pb" && !asset) ||
            entries.contains(name) ||
            mz_zip_reader_get_filename(&archive, index, nullptr, 0) != name.size() + 1 ||
            stat.m_is_directory || stat.m_is_encrypted ||
            ((stat.m_external_attr >> 16) & 0170000) == 0120000)
            throw std::invalid_argument("package.entry_name");
        const auto maximum = name == "manifest.json" ? std::size_t{65536}
                             : asset                 ? kMaximumPackageAssetBytes
                                                     : kMaximumProgramBytes;
        if (stat.m_uncomp_size > maximum) throw std::length_error("package.entry_bytes");
        if (asset) {
            if (stat.m_uncomp_size > kMaximumPackageAssetBytes - asset_bytes)
                throw std::length_error("package.asset_bytes");
            asset_bytes += stat.m_uncomp_size;
        }
        std::string content(static_cast<std::size_t>(stat.m_uncomp_size), '\0');
        // Extract into a bounded value buffer, never to filesystem paths.
        if (!mz_zip_reader_extract_to_mem(&archive, index, content.data(), content.size(), 0))
            throw std::invalid_argument("package.entry_crc");
        entries.emplace(name, std::move(content));
    }
    return entries;
}
}  // namespace rhythm::project::detail
