#include "rhythm/player/package_loader.h"

#include <chrono>

namespace rhythm::player {
namespace {
PackageLoadResult Load(const std::filesystem::path& source,
                       const std::optional<std::filesystem::path>& install, std::stop_token stop) {
    PackageLoadResult result;
    storage::FileBytes bytes;
    try {
        bytes = storage::FileBytes::Open(source, project::kMaximumFilePackageBytes);
        if (!bytes.Size()) return {{}, PackageLoadError::kRead};
    } catch (const std::exception&) {
        return {{}, PackageLoadError::kRead};
    }
    if (stop.stop_requested()) return {{}, PackageLoadError::kCancelled};
    try {
        result.package_.emplace(bytes, stop);
    } catch (const std::exception&) {
        return {{},
                stop.stop_requested() ? PackageLoadError::kCancelled : PackageLoadError::kInvalid};
    }
    if (stop.stop_requested()) return {{}, PackageLoadError::kCancelled};
    if (install) {
        try {
            project::InstallPackageFile(*install, bytes, false, stop);
        } catch (const std::exception&) {
            return {{},
                    stop.stop_requested() ? PackageLoadError::kCancelled
                                          : PackageLoadError::kInstall};
        }
    }
    return result;
}
}  // namespace
PackageLoader::~PackageLoader() {
    Cancel();
    executor_.RequestStop(foundation::ShutdownMode::kDrain);
    executor_.Join();
}
bool PackageLoader::StartFile(std::filesystem::path source,
                              std::optional<std::filesystem::path> install) {
    if (Busy()) return false;
    cancellation_ = {};
    auto task = std::make_shared<std::packaged_task<PackageLoadResult()>>(
            [source = std::move(source), install = std::move(install),
             stop = cancellation_.get_token()] { return Load(source, install, stop); });
    auto completion = task->get_future();
    if (executor_.TryPost([task] { (*task)(); }) != foundation::SubmitResult::kAccepted)
        return false;
    pending_ = std::move(completion);
    return true;
}
void PackageLoader::Cancel() { cancellation_.request_stop(); }
bool PackageLoader::StartBytes(storage::FileBytes source) {
    if (Busy() || !source.Valid() || !source.Size() ||
        source.Size() > project::kMaximumFilePackageBytes)
        return false;
    cancellation_ = {};
    auto task = std::make_shared<std::packaged_task<PackageLoadResult()>>(
            [source = std::move(source), stop = cancellation_.get_token()] {
                try {
                    if (stop.stop_requested())
                        return PackageLoadResult{{}, PackageLoadError::kCancelled};
                    PreparedPackage package(source, stop);
                    if (stop.stop_requested())
                        return PackageLoadResult{{}, PackageLoadError::kCancelled};
                    return PackageLoadResult{std::move(package), PackageLoadError::kNone};
                } catch (const std::exception&) {
                    return PackageLoadResult{{},
                                             stop.stop_requested() ? PackageLoadError::kCancelled
                                                                   : PackageLoadError::kInvalid};
                }
            });
    auto completion = task->get_future();
    if (executor_.TryPost([task] { (*task)(); }) != foundation::SubmitResult::kAccepted)
        return false;
    pending_ = std::move(completion);
    return true;
}
std::optional<PackageLoadResult> PackageLoader::Take() {
    if (!Busy() || pending_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return std::nullopt;
    return pending_.get();
}
}  // namespace rhythm::player
