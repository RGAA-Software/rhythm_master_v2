#include "rhythm/project/async_store.h"

#include <chrono>

#include "rhythm/project/package.h"
namespace rhythm::project {
AsyncStore::~AsyncStore() {
    executor_.RequestStop(foundation::ShutdownMode::kDrain);
    executor_.Join();
}
bool AsyncStore::Submit(std::function<StoreCompletion()> operation) {
    if (Busy()) return false;
    auto task = std::make_shared<std::packaged_task<StoreCompletion()>>(std::move(operation));
    auto completion = task->get_future();
    if (executor_.TryPost([task] { (*task)(); }) != foundation::SubmitResult::kAccepted)
        return false;
    pending_ = std::move(completion);
    return true;
}
bool AsyncStore::PublishProject(std::filesystem::path path, editor::Snapshot snapshot,
                                std::filesystem::path asset_directory) {
    return Submit([path = std::move(path), snapshot = std::move(snapshot),
                   asset_directory = std::move(asset_directory)] {
        StoreCompletion result;
        result.saved_revision_ = snapshot.document_.revision_;
        try {
            PublishSnapshot(path, snapshot, asset_directory);
            result.published_path_ = path;
        } catch (const std::exception& error) {
            result.error_ = error.what();
        }
        return result;
    });
}
bool AsyncStore::SaveProject(std::filesystem::path path, editor::Snapshot snapshot) {
    return Submit([path = std::move(path), snapshot = std::move(snapshot)] {
        StoreCompletion result;
        result.saved_revision_ = snapshot.document_.revision_;
        try {
            Save(path, snapshot);
        } catch (const std::exception& error) {
            result.error_ = error.what();
        }
        return result;
    });
}
bool AsyncStore::LoadProject(std::filesystem::path path) {
    return Submit([path = std::move(path)] {
        StoreCompletion result;
        try {
            result.loaded_ = Load(path);
        } catch (const std::exception& error) {
            result.error_ = error.what();
        }
        return result;
    });
}
std::optional<StoreCompletion> AsyncStore::Take() {
    if (!pending_.valid() ||
        pending_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return std::nullopt;
    return pending_.get();
}
bool AsyncStore::LoadTemplate(std::filesystem::path directory,
                              std::filesystem::path asset_directory) {
    return Submit([directory = std::move(directory), asset_directory = std::move(asset_directory)] {
        StoreCompletion result;
        result.template_ = true;
        try {
            result.loaded_ = PrepareTemplate(directory, asset_directory);
        } catch (const std::exception& error) {
            result.error_ = error.what();
        }
        return result;
    });
}
}  // namespace rhythm::project
