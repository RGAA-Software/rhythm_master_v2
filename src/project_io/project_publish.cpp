#include <optional>
#include <stdexcept>

#include "rhythm/assets/store.h"
#include "rhythm/prepared_assets/prepare.h"
#include "rhythm/project/package.h"
#include "rhythm/project/store.h"

namespace rhythm::project {
void PublishSnapshot(const std::filesystem::path& path, const editor::Snapshot& snapshot,
                     const std::filesystem::path& asset_directory) {
    const bool streamed = RequiresStreamedAudio(snapshot.assets_, snapshot.soundtrack_);
    std::vector<PackagedAsset> packaged;
    std::optional<RuntimePackage::StreamedAudio> music;
    if (!snapshot.assets_.empty()) {
        if (asset_directory.empty() || !std::filesystem::is_directory(asset_directory))
            throw std::invalid_argument("package.asset_directory");
        const assets::Store store(asset_directory);
        std::size_t remaining = kMaximumPackageAssetBytes;
        for (const auto& record : snapshot.assets_) {
            if (streamed && record.id_ == snapshot.soundtrack_->asset_) {
                music = RuntimePackage::StreamedAudio{record,
                                                      store.Open(record, kMaximumMusicAssetBytes)};
                continue;
            }
            auto bytes = store.Read(record, remaining);
            remaining -= bytes.size();
            packaged.push_back({record, std::move(bytes)});
        }
    }
    const auto compiled = graph::Compile(snapshot.document_, graph::Registry{});
    if (!std::holds_alternative<graph::ExecutionPlan>(compiled))
        throw std::invalid_argument("package.invalid_program");
    prepared_assets::Prepare(std::get<graph::ExecutionPlan>(compiled), packaged);
    if (music) {
        RuntimePackage probe;
        probe.profile_ = PackageProfile::kMusicPerformanceV2;
        probe.soundtrack_ = snapshot.soundtrack_;
        probe.streamed_audio_ = music;
        prepared_assets::PrepareSoundtrack(probe);
        PublishMusicPackage(path, snapshot.document_, snapshot.title_, packaged, *music,
                            *snapshot.soundtrack_);
        return;
    }
    prepared_assets::PrepareSoundtrack(snapshot.soundtrack_, packaged);
    InstallPackage(path, EncodePackage(snapshot.document_, snapshot.title_, packaged,
                                       snapshot.soundtrack_));
}
}  // namespace rhythm::project
