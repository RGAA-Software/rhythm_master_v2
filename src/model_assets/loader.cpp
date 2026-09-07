#include "rhythm/model_assets/loader.h"

#include <chrono>
#include <set>

#include "rhythm/assets/store.h"

namespace rhythm::model_assets {
namespace {
LoadResult Load(LoadRequest request, std::stop_token stop) {
    LoadResult result;
    result.generation_ = request.generation_;
    result.plan_ = std::move(request.plan_);
    try {
        const assets::Store store(std::move(request.directory_));
        std::vector<project::PackagedAsset> packaged;
        std::set<std::string> required;
        for (const auto& instruction : result.plan_.instructions_)
            if (instruction.operation_ == graph::Operation::kGeometryGlb)
                required.insert(std::get<assets::AssetId>(instruction.node_.properties_.at("asset"))
                                        .sha256_);
        std::size_t remaining = project::kMaximumPackageAssetBytes;
        for (const auto& record : request.assets_) {
            if (stop.stop_requested()) throw std::runtime_error("model.cancelled");
            if (!required.contains(record.id_.sha256_)) continue;
            auto bytes = store.Read(record, remaining);
            remaining -= bytes.size();
            packaged.push_back({record, std::move(bytes)});
        }
        result.resources_ = Prepare(result.plan_, packaged, stop);
    } catch (const std::exception& error) {
        result.error_ = error.what();
    }
    return result;
}
}  // namespace
Loader::~Loader() {
    Cancel();
    executor_.RequestStop(foundation::ShutdownMode::kDrain);
    executor_.Join();
}
void Loader::Cancel() {
    cancellation_.request_stop();
    latest_.reset();
}
void Loader::Submit(LoadRequest request) {
    if (request.assets_.size() > project::kMaximumPackageAssets ||
        request.plan_.instructions_.size() > 10000)
        throw std::length_error("model.asset_count");
    Cancel();
    latest_ = std::move(request);
    if (!pending_.valid()) StartLatest();
}
void Loader::StartLatest() {
    if (!latest_) return;
    cancellation_ = {};
    auto task = std::make_shared<std::packaged_task<LoadResult()>>(
            [request = std::move(*latest_), stop = cancellation_.get_token()]() mutable {
                return Load(std::move(request), stop);
            });
    latest_.reset();
    auto completion = task->get_future();
    if (executor_.TryPost([task] { (*task)(); }) != foundation::SubmitResult::kAccepted)
        throw std::runtime_error("model.worker_unavailable");
    pending_ = std::move(completion);
}
std::optional<LoadResult> Loader::Take() {
    if (!pending_.valid() ||
        pending_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return {};
    auto result = pending_.get();
    const auto cancelled = cancellation_.stop_requested();
    StartLatest();
    if (cancelled) return {};
    return result;
}
}  // namespace rhythm::model_assets
