#include "rhythm/player/work_library.h"

#include <stdexcept>
#include <utility>

#include "rhythm/assets/store.h"
#include "rhythm/project/package.h"

namespace rhythm::player {
namespace {
constexpr auto kPackageMediaType = "application/vnd.rhythm.package";
}  // namespace
WorkLibrary::WorkLibrary(std::filesystem::path directory) : directory_(std::move(directory)) {
    std::filesystem::create_directories(directory_);
    directory_ = std::filesystem::canonical(directory_);
}
StoredWork WorkLibrary::Import(const std::filesystem::path& source,
                               const std::optional<performance::WorkReference>& expected,
                               std::stop_token stop) {
    if (expected && !performance::ValidWork(*expected))
        throw std::invalid_argument("performance.work");
    // Reject ordinary corrupt imports before copying. The copied immutable bytes
    // are validated again, because external sources may change while importing.
    (void)project::LoadPackage(source, stop);
    assets::Store store(directory_);
    performance::WorkReference reference;
    if (expected) {
        reference = *expected;
        const auto size = std::filesystem::file_size(source);
        if (size > project::kMaximumFilePackageBytes)
            throw std::length_error("performance.package_limit");
        store.Restore(source, {reference.package_, size, kPackageMediaType}, stop);
    } else {
        const auto record =
                store.Import(source, kPackageMediaType, project::kMaximumFilePackageBytes, stop);
        reference = {performance::WorkSource::kManaged, {}, {}, record.id_};
    }
    const auto package = project::ReadPackage(Open(reference, stop), stop);
    return {std::move(reference), package.title_};
}
storage::FileBytes WorkLibrary::Open(const performance::WorkReference& work,
                                     std::stop_token stop) const {
    if (!performance::ValidWork(work)) throw std::invalid_argument("performance.work");
    return assets::Store(directory_).OpenId(work.package_, project::kMaximumFilePackageBytes, stop);
}
}  // namespace rhythm::player
