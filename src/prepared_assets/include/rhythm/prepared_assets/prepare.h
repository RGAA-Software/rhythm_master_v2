#pragma once

#include "rhythm/assets/images.h"
#include "rhythm/media/video_frame.h"
#include "rhythm/model_assets/prepare.h"

namespace rhythm::prepared_assets {
struct VideoSource {
    assets::AssetId id_{};
    std::shared_ptr<const std::vector<std::uint8_t>> bytes_{};
    std::shared_ptr<const media::VideoFrame> first_{};
};
// A worker publishes the complete immutable resource set in one transaction.
// Domain/runtime consumers receive project values, never decoder resources.
struct Resources {
    std::shared_ptr<const scene::Resources> models_ = std::make_shared<const scene::Resources>();
    std::shared_ptr<const assets::Images> images_ = std::make_shared<const assets::Images>();
    std::vector<VideoSource> videos_{};
};
std::shared_ptr<const Resources> Prepare(const graph::ExecutionPlan& plan,
                                         std::span<const project::PackagedAsset> assets,
                                         std::stop_token stop = {});
bool Covers(const graph::ExecutionPlan& plan, const Resources& resources);
// Worker-only: probes the actual embedded audio with the shared media backend.
std::optional<media::SoundtrackSource> PrepareSoundtrack(
        const std::optional<media::Soundtrack>& binding,
        std::span<const project::PackagedAsset> assets, std::stop_token stop = {});
}  // namespace rhythm::prepared_assets
