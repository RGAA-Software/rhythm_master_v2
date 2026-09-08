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
    return StartTask([directory = std::move(directory), source = std::move(source),
                      media_type = std::move(media_type), maximum_bytes](std::stop_token stop) {
        ImportResult result;
        result.asset_ = Store(directory).Import(source, media_type, maximum_bytes, stop);
        return result;
    });
}
bool Importer::StartInspect(std::filesystem::path directory, std::vector<AssetRecord> records) {
    if (records.size() > 64) return false;
    std::uint64_t total = 0;
    for (const auto& record : records) {
        if (record.bytes_ > 1024ULL * 1024 * 1024 - total) return false;
        total += record.bytes_;
    }
    return StartTask(
            [directory = std::move(directory), records = std::move(records)](std::stop_token stop) {
                const Store store(directory);
                ImportResult result;
                for (const auto& record : records)
                    result.checks_.push_back({record, store.Inspect(record, stop)});
                return result;
            });
}
bool Importer::StartRestore(std::filesystem::path directory, std::filesystem::path source,
                            AssetRecord expected) {
    return StartTask([directory = std::move(directory), source = std::move(source),
                      expected = std::move(expected)](std::stop_token stop) {
        Store(directory).Restore(source, expected, stop);
        ImportResult result;
        result.restored_ = expected;
        return result;
    });
}
bool Importer::StartTask(std::function<ImportResult(std::stop_token)> operation) {
    if (Busy()) return false;
    cancellation_ = std::stop_source{};
    auto task = std::make_shared<std::packaged_task<ImportResult()>>(
            [operation = std::move(operation), stop = cancellation_.get_token()] {
                try {
                    return operation(stop);
                } catch (const std::exception& error) {
                    ImportResult result;
                    result.error_ = error.what();
                    return result;
                }
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
