#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace rhythm::media {
struct VideoInfo {
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    double pixel_aspect_ = 1;
    double clockwise_rotation_ = 0;
    std::optional<double> duration_seconds_{};
};

struct VideoFrame {
    // Owned, tightly packed straight-alpha RGBA8. Display orientation and pixel
    // aspect are retained separately. The first profile supports SDR only.
    VideoInfo info_{};
    std::vector<std::uint8_t> rgba_{};
    double seconds_ = 0;
    double duration_seconds_ = 0;
    std::uint64_t generation_ = 0;
};

}  // namespace rhythm::media
