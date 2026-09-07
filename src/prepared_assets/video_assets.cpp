#include "video_assets.h"

#include <algorithm>
#include <set>
#include <stdexcept>

#if defined(RHYTHM_HAS_IMAGE_DECODER)
#include "rhythm/media/video_decoder.h"
#endif

namespace rhythm::prepared_assets::detail {
std::vector<VideoSource> PrepareVideos(const graph::ExecutionPlan& plan,
                                       std::span<const project::PackagedAsset> assets,
                                       std::stop_token stop) {
    std::vector<VideoSource> result;
    std::set<std::string> seen;
    std::size_t instances = 0;
    for (const auto& instruction : plan.instructions_) {
        if (stop.stop_requested()) throw std::runtime_error("asset.cancelled");
        if (instruction.operation_ != graph::Operation::kTextureVideo) continue;
        if (++instances > 4) throw std::length_error("video.instance_budget");
        const auto& id = std::get<assets::AssetId>(instruction.node_.properties_.at("asset"));
        if (!assets::ValidId(id)) throw std::invalid_argument("video.asset_missing");
        if (!seen.insert(id.sha256_).second) continue;
        const auto found = std::find_if(assets.begin(), assets.end(),
                                        [&](const auto& asset) { return asset.record_.id_ == id; });
        if (found == assets.end()) throw std::invalid_argument("video.asset_missing");
        const auto& mime = found->record_.media_type_;
        if (mime != "video/mp4" && mime != "video/x-matroska" && mime != "video/webm" &&
            mime != "video/quicktime")
            throw std::invalid_argument("video.media_type");
#if defined(RHYTHM_HAS_IMAGE_DECODER)
        auto bytes = std::make_shared<const std::vector<std::uint8_t>>(found->bytes_.begin(),
                                                                       found->bytes_.end());
        media::VideoDecoder decoder(std::span<const std::uint8_t>(*bytes), 0, stop);
        auto first = decoder.Read(stop);
        if (!first) throw std::invalid_argument("video.no_frames");
        result.push_back({id, std::move(bytes),
                          std::make_shared<const media::VideoFrame>(std::move(*first))});
#else
        throw std::runtime_error("video.decoder_unavailable");
#endif
    }
    return result;
}
}  // namespace rhythm::prepared_assets::detail
