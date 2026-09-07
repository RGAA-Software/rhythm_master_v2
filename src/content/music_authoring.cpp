#include "rhythm/content/music_authoring.h"

#include <chrono>
#include <stdexcept>

#include "rhythm/assets/store.h"
#include "rhythm/prepared_assets/prepare.h"
#include "rhythm/project/package.h"

namespace rhythm::content {
namespace {
bool UsesAsset(std::span<const graph::Node> nodes, const assets::AssetId& asset) {
    for (const auto& node : nodes)
        for (const auto& [key, value] : node.properties_)
            if (std::holds_alternative<assets::AssetId>(value) &&
                std::get<assets::AssetId>(value) == asset)
                return true;
    return false;
}
MusicImportResult Import(editor::Snapshot snapshot, const std::filesystem::path& directory,
                         const std::filesystem::path& source, float gain, bool loop,
                         std::stop_token stop) {
    MusicImportResult result{snapshot.document_.id_, snapshot.document_.revision_};
    try {
        if (!std::isfinite(gain) || gain < 0 || gain > 1)
            throw std::invalid_argument("project.soundtrack_invalid");
        auto next = UnbindSoundtrack(std::move(snapshot));
        assets::Store store(directory);
        // This private media type denotes audio recognized by FFmpeg, independent
        // of user filename extensions. No second decoder/protocol is selected.
        const auto record = store.Import(source, "audio/x-rhythm-media",
                                         project::kMaximumMusicAssetBytes, stop);
        const auto existing =
                std::find_if(next.assets_.begin(), next.assets_.end(),
                             [&](const auto& item) { return item.id_ == record.id_; });
        if (existing == next.assets_.end())
            next.assets_.push_back(record);
        else if (!existing->media_type_.starts_with("audio/"))
            throw std::invalid_argument("project.soundtrack_invalid");
        const auto title = source.filename().u8string();
        next.soundtrack_ =
                media::Soundtrack{record.id_, std::string(title.begin(), title.end()), gain, loop};
        if (!media::ValidSoundtrack(*next.soundtrack_, next.assets_))
            throw std::invalid_argument("project.soundtrack_invalid");
        if (project::RequiresStreamedAudio(next.assets_, next.soundtrack_)) {
            bool used = UsesAsset(next.document_.nodes_, record.id_);
            for (const auto& component : next.document_.components_)
                used |= UsesAsset(component.nodes_, record.id_);
            if (used) throw std::length_error("package.asset_bytes");
        }
        project::RuntimePackage probe;
        probe.profile_ = project::PackageProfile::kMusicPerformanceV2;
        probe.soundtrack_ = next.soundtrack_;
        probe.streamed_audio_ = project::RuntimePackage::StreamedAudio{
                record, store.Open(record, project::kMaximumMusicAssetBytes, stop)};
        prepared_assets::PrepareSoundtrack(probe, stop);
        if (stop.stop_requested()) throw std::runtime_error("audio.canceled");
        result.snapshot_ = std::move(next);
    } catch (const std::exception& error) {
        result.error_ = stop.stop_requested() ? "music.binding_canceled" : error.what();
    }
    return result;
}
}  // namespace
editor::Snapshot UnbindSoundtrack(editor::Snapshot snapshot) {
    if (!snapshot.soundtrack_) return snapshot;
    const auto id = snapshot.soundtrack_->asset_;
    bool used = UsesAsset(snapshot.document_.nodes_, id);
    for (const auto& component : snapshot.document_.components_)
        used |= UsesAsset(component.nodes_, id);
    if (!used)
        std::erase_if(snapshot.assets_, [&](const auto& record) { return record.id_ == id; });
    snapshot.soundtrack_.reset();
    return snapshot;
}
MusicAuthoring::~MusicAuthoring() {
    Cancel();
    executor_.RequestStop(foundation::ShutdownMode::kDrain);
    executor_.Join();
}
bool MusicAuthoring::Start(editor::Snapshot snapshot, std::filesystem::path assets,
                           std::filesystem::path source, float gain, bool loop) {
    if (Busy()) return false;
    cancellation_ = {};
    auto task = std::make_shared<std::packaged_task<MusicImportResult()>>(
            [snapshot = std::move(snapshot), assets = std::move(assets), source = std::move(source),
             gain, loop, stop = cancellation_.get_token()]() mutable {
                return Import(std::move(snapshot), assets, source, gain, loop, stop);
            });
    auto completion = task->get_future();
    if (executor_.TryPost([task] { (*task)(); }) != foundation::SubmitResult::kAccepted)
        return false;
    pending_ = std::move(completion);
    return true;
}
void MusicAuthoring::Cancel() { cancellation_.request_stop(); }
std::optional<MusicImportResult> MusicAuthoring::Take() {
    if (!pending_.valid() ||
        pending_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return {};
    auto result = pending_.get();
    if (cancellation_.stop_requested()) {
        result.snapshot_.reset();
        result.error_ = "music.binding_canceled";
    }
    return result;
}
}  // namespace rhythm::content
