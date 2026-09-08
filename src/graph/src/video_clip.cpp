#include "rhythm/graph/video_clip.h"

#include <cmath>
#include <stdexcept>

namespace rhythm::graph {
parameters::ClipInterval DescribeVideoClip(const Node& node) {
    if (node.type_ != "texture.video_clip") throw std::invalid_argument("clip.node_type");
    const auto end = Scalar(node, "clip_end", 0);
    if (!std::isfinite(end) || end < 0 || end > 2 || std::floor(end) != end)
        throw std::invalid_argument("clip.interval");
    return parameters::ClipInterval({Scalar(node, "clip_start", 0),
                                     Scalar(node, "clip_duration", 1), Scalar(node, "source_in", 0),
                                     Scalar(node, "source_out", 1), Scalar(node, "clip_rate", 1),
                                     static_cast<parameters::ClipEnd>(static_cast<int>(end)),
                                     Scalar(node, "fade_in", 0), Scalar(node, "fade_out", 0),
                                     Scalar(node, "fade_shape", 1) != 0});
}
}  // namespace rhythm::graph
