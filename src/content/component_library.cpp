#include "rhythm/content/component_library.h"

#include <algorithm>
#include <chrono>

namespace rhythm::content {
namespace {
LibraryResult Scan(const std::filesystem::path& directory) {
    LibraryResult result;
    result.entries_.emplace();
    std::filesystem::create_directories(directory);
    std::size_t inspected = 0;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (++inspected > 1024 || result.entries_->size() == 256) {
            result.error_ = "component.library_limit";
            break;
        }
        if (entry.is_symlink() || !entry.is_directory() ||
            entry.path().extension() != ".rhythmcomponent")
            continue;
        try {
            const auto component = project::Load(entry.path());
            if (component.snapshot_.document_.id_ != "component.library")
                throw std::invalid_argument("component.library_invalid");
            result.entries_->push_back({entry.path(), component.snapshot_.title_});
        } catch (const std::exception&) {
            result.error_ = "component.library_skipped";
        }
    }
    std::sort(result.entries_->begin(), result.entries_->end(),
              [](const auto& a, const auto& b) { return a.title_ < b.title_; });
    return result;
}
}  // namespace
ComponentLibrary::ComponentLibrary(std::filesystem::path directory)
    : directory_(std::move(directory)) {
    Refresh();
}
ComponentLibrary::~ComponentLibrary() {
    executor_.RequestStop(foundation::ShutdownMode::kDrain);
    executor_.Join();
}
bool ComponentLibrary::Submit(std::function<LibraryResult()> operation) {
    if (Busy()) return false;
    auto task = std::make_shared<std::packaged_task<LibraryResult()>>(
            [operation = std::move(operation)] {
                try {
                    return operation();
                } catch (const std::exception& error) {
                    LibraryResult result;
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
bool ComponentLibrary::Refresh() {
    return Submit([directory = directory_] { return Scan(directory); });
}
bool ComponentLibrary::Save(editor::Snapshot snapshot, graph::NodeId instance,
                            std::filesystem::path source_assets) {
    return Submit([directory = directory_, snapshot = std::move(snapshot), instance,
                   source_assets = std::move(source_assets)] {
        const auto captured = CaptureComponent(snapshot, instance, graph::Registry{});
        const auto saved = SaveComponent(directory, captured, source_assets);
        auto result = Scan(directory);
        result.saved_ = saved;
        return result;
    });
}
bool ComponentLibrary::Load(std::filesystem::path directory,
                            std::filesystem::path destination_assets, std::string expected_document,
                            std::uint64_t expected_revision) {
    return Submit([directory = std::move(directory),
                   destination_assets = std::move(destination_assets),
                   expected_document = std::move(expected_document), expected_revision] {
        LibraryResult result;
        result.component_ = LoadComponent(directory, destination_assets);
        result.expected_document_ = expected_document;
        result.expected_revision_ = expected_revision;
        return result;
    });
}
std::optional<LibraryResult> ComponentLibrary::Take() {
    if (!pending_.valid() ||
        pending_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return {};
    return pending_.get();
}
bool ComponentLibrary::LoadOfficial(Semantic semantic, std::filesystem::path destination_assets,
                                    std::string expected_document,
                                    std::uint64_t expected_revision) {
    return Submit([semantic = std::move(semantic),
                   destination_assets = std::move(destination_assets),
                   expected_document = std::move(expected_document), expected_revision] {
        LibraryResult result;
        result.component_ = LoadOfficialComponent(semantic, destination_assets);
        result.expected_document_ = expected_document;
        result.expected_revision_ = expected_revision;
        return result;
    });
}
}  // namespace rhythm::content
