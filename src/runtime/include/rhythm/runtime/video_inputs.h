#pragma once

#include <memory>

#include "rhythm/graph/document.h"
#include "rhythm/media/video_frame.h"

namespace rhythm::runtime {
struct VideoInput {
    graph::NodeId node_ = 0;
    assets::AssetId source_{};
    std::shared_ptr<const media::VideoFrame> frame_{};
    std::uint64_t revision_ = 0;
    std::uint64_t generation_ = 0;
    // Straight-alpha opacity; changing it does not invalidate decoded pixels.
    double gain_ = 1;
    bool operator==(const VideoInput&) const = default;
};
}  // namespace rhythm::runtime
