#include "rhythm/assets/importer.h"

#include <chrono>

namespace rhythm::assets {
Importer::~Importer() {
    Cancel();
    executor_.RequestStop(foundation::ShutdownMode::kDrain);
    executor_.Join();
}
bool Importer::Start(std::filesystem::path directory, std::filesystem::path source,
                     std::string media_type, std::uint64_t maximum_bytes) {
    if (Busy()) return false;
    cancellation_ = std::stop_source{};
    auto task = std::make_shared<std::packaged_task<ImportResult()>>(
            [directory = std::move(directory), source = std::move(source),
             media_type = std::move(media_type), maximum_bytes,
             cancellation = cancellation_.get_token()] {
                ImportResult result;
                try {
                    result.asset_ = Store(directory).Import(source, media_type, maximum_bytes,
                                                            cancellation);
                } catch (const std::exception& error) {
                    result.error_ = error.what();
                }
                return result;
            });
    auto completion = task->get_future();
    if (executor_.TryPost([task] { (*task)(); }) != foundation::SubmitResult::kAccepted)
        return false;
    pending_ = std::move(completion);
    return true;
}
void Importer::Cancel() { cancellation_.request_stop(); }
std::optional<ImportResult> Importer::Take() {
    if (!pending_.valid() ||
        pending_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return std::nullopt;
    return pending_.get();
}
}  // namespace rhythm::assets
