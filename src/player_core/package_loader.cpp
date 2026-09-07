#include "rhythm/player/package_loader.h"

#include <algorithm>
#include <chrono>
#include <fstream>

namespace rhythm::player {
namespace {
PackageLoadResult Load(const std::filesystem::path& source,
                       const std::optional<std::filesystem::path>& install, std::stop_token stop) {
    PackageLoadResult result;
    std::string bytes;
    try {
        const auto size = std::filesystem::file_size(source);
        if (!size || size > project::kMaximumPackageBytes) return {{}, PackageLoadError::kRead};
        std::ifstream input(source, std::ios::binary);
        if (!input) return {{}, PackageLoadError::kRead};
        bytes.resize(static_cast<std::size_t>(size));
        for (std::size_t offset = 0; offset < bytes.size();) {
            if (stop.stop_requested()) return {{}, PackageLoadError::kCancelled};
            const auto count = static_cast<std::streamsize>(
                    std::min<std::size_t>(65536, bytes.size() - offset));
            if (!input.read(bytes.data() + offset, count)) return {{}, PackageLoadError::kRead};
            offset += static_cast<std::size_t>(count);
        }
        if (input.peek() != std::char_traits<char>::eof()) return {{}, PackageLoadError::kRead};
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
            project::InstallPackage(*install, bytes);
        } catch (const std::exception&) {
            return {{}, PackageLoadError::kInstall};
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
std::optional<PackageLoadResult> PackageLoader::Take() {
    if (!Busy() || pending_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return std::nullopt;
    return pending_.get();
}
}  // namespace rhythm::player
