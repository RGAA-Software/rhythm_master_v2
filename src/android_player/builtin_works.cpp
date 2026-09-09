#include "builtin_works.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <fstream>
#include <memory>
#include <nlohmann/json.hpp>
#include <set>
#include <stdexcept>

#include "rhythm/project/package.h"

namespace rhythm::android_host {
namespace {
struct StreamCloser {
    void operator()(SDL_IOStream* stream) const { SDL_CloseIO(stream); }
};
class StagedBuiltin final {
   public:
    explicit StagedBuiltin(std::filesystem::path path) : path_(std::move(path)) {}
    ~StagedBuiltin() {
        std::error_code ignored;
        std::filesystem::remove(path_, ignored);
    }
    StagedBuiltin(const StagedBuiltin&) = delete;
    StagedBuiltin& operator=(const StagedBuiltin&) = delete;
    const std::filesystem::path& Path() const { return path_; }

   private:
    std::filesystem::path path_{};
};
void CopyAsset(const std::string& asset, const std::filesystem::path& destination,
               std::stop_token stop) {
    // SDL_IOFromFile transfers ownership to this adapter. On Android, relative
    // names open APK assets; reuse the existing Host::ReadAsset boundary in chunks.
    std::unique_ptr<SDL_IOStream, StreamCloser> stream(SDL_IOFromFile(asset.c_str(), "rb"));
    if (!stream) throw std::runtime_error("performance.builtin_missing");
    const auto size = SDL_GetIOSize(stream.get());
    if (size < 1 || static_cast<std::uint64_t>(size) > project::kMaximumFilePackageBytes)
        throw std::length_error("performance.package_limit");
    std::ofstream output(destination, std::ios::binary | std::ios::trunc);
    output.exceptions(std::ios::badbit | std::ios::failbit);
    std::array<char, 65536> buffer{};
    std::uint64_t copied = 0;
    while (copied < static_cast<std::uint64_t>(size)) {
        if (stop.stop_requested()) throw std::runtime_error("performance.cancelled");
        const auto count =
                static_cast<std::size_t>(std::min<std::uint64_t>(buffer.size(), size - copied));
        if (SDL_ReadIO(stream.get(), buffer.data(), count) != count)
            throw std::runtime_error("performance.builtin_read");
        output.write(buffer.data(), static_cast<std::streamsize>(count));
        copied += count;
    }
    output.flush();
}
}  // namespace
std::vector<BuiltinWork> ReadBuiltinWorks(std::string_view catalog) {
    if (catalog.size() > 256 * 1024) throw std::length_error("performance.catalog_limit");
    const auto records = nlohmann::json::parse(catalog);
    if (!records.is_array() || records.size() > 256)
        throw std::length_error("performance.catalog_limit");
    std::vector<BuiltinWork> works;
    std::set<std::string> ids;
    for (const auto& record : records) {
        BuiltinWork work;
        work.reference_ = {performance::WorkSource::kBuiltin,
                           record.at("content_id").get<std::string>(),
                           record.at("content_version").get<std::string>(),
                           {record.at("sha256").get<std::string>()},
                           performance::VersionPolicy::kCurrentBuiltin};
        const auto id = record.at("id").get<std::string>();
        if (id.empty() ||
            !std::all_of(id.begin(), id.end(),
                         [](char value) {
                             return (value >= 'a' && value <= 'z') ||
                                    (value >= '0' && value <= '9') || value == '_';
                         }) ||
            !performance::ValidWork(work.reference_) ||
            !ids.insert(work.reference_.content_id_).second)
            throw std::invalid_argument("performance.catalog");
        work.asset_ = record.at("package").get<std::string>();
        if (work.asset_ != "effects/" + id + ".rhythmpack")
            throw std::invalid_argument("performance.catalog_path");
        works.push_back(std::move(work));
    }
    return works;
}
player::BuiltinWorkReader BuiltinReader(std::filesystem::path directory,
                                        std::vector<BuiltinWork> works) {
    return [directory = std::move(directory), works = std::move(works)](
                   const performance::WorkReference& reference, std::stop_token stop) {
        const auto found = std::find_if(works.begin(), works.end(), [&](const auto& work) {
            return work.reference_.content_id_ == reference.content_id_ &&
                   work.reference_.version_ == reference.version_ &&
                   work.reference_.package_ == reference.package_;
        });
        if (found == works.end()) throw std::runtime_error("performance.builtin_missing");
        player::WorkLibrary library(directory / "works");
        try {
            return library.Open(reference, stop);
        } catch (const std::exception&) {
            if (stop.stop_requested()) throw;
        }
        const auto path = directory / "builtin.pending";
        if (std::filesystem::is_symlink(path)) throw std::invalid_argument("performance.symlink");
        const StagedBuiltin staged(path);
        CopyAsset(found->asset_, staged.Path(), stop);
        library.Import(staged.Path(), reference, stop);
        return library.Open(reference, stop);
    };
}
}  // namespace rhythm::android_host
